#include "PluginProcessor.h"
#include "PluginEditor.h"

MultibandConvolverAudioProcessor::MultibandConvolverAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", Params::createParameterLayout())
{
    for (int b = 0; b < maxBands; ++b)
        bandChains[(size_t) b] = std::make_unique<BandChain>(convolutionQueue, reshapeWorker);

    numBandsParam = apvts.getRawParameterValue(Params::numBandsID());
    inputGainParam = apvts.getRawParameterValue(Params::inputGainID());
    outputGainParam = apvts.getRawParameterValue(Params::outputGainID());

    for (int s = 0; s < Params::maxSplitPoints; ++s)
        splitHzParams[(size_t) s] = apvts.getRawParameterValue(Params::splitHzID(s));

    for (int b = 0; b < maxBands; ++b)
    {
        auto& p = bandParams[(size_t) b];
        p.irIndex = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(Params::bandIrIndexID(b)));
        p.dryWet = apvts.getRawParameterValue(Params::bandDryWetID(b));
        p.preDelay = apvts.getRawParameterValue(Params::bandPreDelayID(b));
        p.tone = apvts.getRawParameterValue(Params::bandToneID(b));
        p.fadeIn = apvts.getRawParameterValue(Params::bandFadeInID(b));
        p.fadeOut = apvts.getRawParameterValue(Params::bandFadeOutID(b));
        p.stretch = apvts.getRawParameterValue(Params::bandStretchID(b));
        p.feedback = apvts.getRawParameterValue(Params::bandFeedbackID(b));
        p.outGain = apvts.getRawParameterValue(Params::bandOutGainID(b));
        p.bypass = apvts.getRawParameterValue(Params::bandBypassID(b));
        p.solo = apvts.getRawParameterValue(Params::bandSoloID(b));
        p.mute = apvts.getRawParameterValue(Params::bandMuteID(b));
    }
}

MultibandConvolverAudioProcessor::~MultibandConvolverAudioProcessor()
{
    // Must happen before bandChains are torn down (see the reshapeWorker member comment in the
    // header) -- guarantees the background thread is fully joined and can't call back into a
    // BandChain that's mid-destruction.
    reshapeWorker.shutdown();
}

const juce::String MultibandConvolverAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

double MultibandConvolverAudioProcessor::getTailLengthSeconds() const
{
    return 5.0;
}

bool MultibandConvolverAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return mainIn == stereo || mainIn == mono;
}

void MultibandConvolverAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumInputChannels();

    splitter.prepare(spec);
    for (auto& chain : bandChains)
        chain->prepare(spec);

    for (auto& buf : bandBuffers)
        buf.setSize((int) spec.numChannels, samplesPerBlock);
}

void MultibandConvolverAudioProcessor::releaseResources()
{
}

void MultibandConvolverAudioProcessor::updateBandMacrosFromParameters()
{
    const int numBands = juce::jlimit(1, maxBands, (int) numBandsParam->load());
    splitter.setNumBands(numBands);

    for (int s = 0; s < Params::maxSplitPoints; ++s)
        splitter.setSplitHz(s, splitHzParams[(size_t) s]->load());

    for (int b = 0; b < numBands; ++b)
    {
        auto& p = bandParams[(size_t) b];

        BandChain::MacroValues values;
        values.irIndex = p.irIndex != nullptr ? p.irIndex->getIndex() : 0;
        values.dryWetPercent = p.dryWet->load();
        values.preDelayMs = p.preDelay->load();
        values.tone = p.tone->load();
        values.fadeInMs = p.fadeIn->load();
        values.fadeOutPercent = p.fadeOut->load();
        values.stretch = p.stretch->load();
        values.feedbackPercent = p.feedback->load();
        values.outputGainDb = p.outGain->load();
        values.bypass = p.bypass->load() > 0.5f;

        bandChains[(size_t) b]->setMacroValues(values);
    }
}

void MultibandConvolverAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);

    buffer.applyGain(juce::Decibels::decibelsToGain(inputGainParam->load()));
    inputAnalyzer.pushSamples(buffer);

    updateBandMacrosFromParameters();

    const int numBands = splitter.getNumBands();
    splitter.process(buffer, bandBuffers);

    // Solo logic: if any active band is soloed, only soloed bands are audible; muted bands are
    // always silent regardless of solo state.
    bool anySolo = false;
    for (int b = 0; b < numBands; ++b)
        if (bandParams[(size_t) b].solo->load() > 0.5f)
            anySolo = true;

    buffer.clear();

    for (int b = 0; b < numBands; ++b)
    {
        auto& band = bandBuffers[(size_t) b];
        const bool isSolo = bandParams[(size_t) b].solo->load() > 0.5f;
        const bool isMuted = bandParams[(size_t) b].mute->load() > 0.5f;
        const bool audible = ! isMuted && (! anySolo || isSolo);

        bandChains[(size_t) b]->process(band);

        if (audible)
        {
            for (int ch = 0; ch < numChannels && ch < band.getNumChannels(); ++ch)
                buffer.addFrom(ch, 0, band, ch, 0, numSamples);
        }
    }

    buffer.applyGain(juce::Decibels::decibelsToGain(outputGainParam->load()));
}

void MultibandConvolverAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MultibandConvolverAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* MultibandConvolverAudioProcessor::createEditor()
{
    return new MultibandConvolverAudioProcessorEditor(*this);
}

void MultibandConvolverAudioProcessor::resetAllParametersToDefault()
{
    for (auto* param : getParameters())
        param->setValueNotifyingHost(param->getDefaultValue());
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MultibandConvolverAudioProcessor();
}
