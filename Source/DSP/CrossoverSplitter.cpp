#include "CrossoverSplitter.h"

CrossoverSplitter::CrossoverSplitter()
{
    for (auto& stage : stages)
    {
        stage.lowFilter.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
        stage.highFilter.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    }
}

void CrossoverSplitter::prepare(const juce::dsp::ProcessSpec& newSpec)
{
    spec = newSpec;

    remaining.setSize((int) spec.numChannels, (int) spec.maximumBlockSize);
    lowScratch.setSize((int) spec.numChannels, (int) spec.maximumBlockSize);

    for (auto& stage : stages)
    {
        stage.lowFilter.prepare(spec);
        stage.highFilter.prepare(spec);
        stage.smoothedHz.reset(spec.sampleRate, 0.05);
        stage.needsReset = true;
    }
}

void CrossoverSplitter::reset()
{
    for (auto& stage : stages)
    {
        stage.lowFilter.reset();
        stage.highFilter.reset();
        stage.needsReset = false;
    }
}

void CrossoverSplitter::setNumBands(int newNumBands)
{
    newNumBands = juce::jlimit(1, Params::maxBands, newNumBands);

    if (newNumBands > numBands)
    {
        // Only the stages newly entering the active range need a state reset -- stages that were
        // already running stay untouched so we don't introduce a blip on every band-count change.
        for (int s = juce::jmax(0, numBands - 1); s <= newNumBands - 2; ++s)
            stages[(size_t) s].needsReset = true;
    }

    numBands = newNumBands;
}

void CrossoverSplitter::setSplitHz(int index, float hz)
{
    jassert(index >= 0 && index < Params::maxSplitPoints);
    stages[(size_t) index].smoothedHz.setTargetValue(hz);
}

void CrossoverSplitter::process(const juce::AudioBuffer<float>& input,
                                 std::array<juce::AudioBuffer<float>, (size_t) Params::maxBands>& bandOutputs)
{
    const int numCh = input.getNumChannels();
    const int numSamples = input.getNumSamples();
    const float nyquistGuard = (float) (spec.sampleRate * 0.49);

    remaining.setSize(numCh, numSamples, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
        remaining.copyFrom(ch, 0, input, ch, 0, numSamples);

    for (int s = 0; s < numBands - 1; ++s)
    {
        auto& stage = stages[(size_t) s];

        if (stage.needsReset)
        {
            stage.lowFilter.reset();
            stage.highFilter.reset();
            stage.needsReset = false;
        }

        stage.smoothedHz.skip(juce::jmax(0, numSamples - 1));
        const float hz = juce::jlimit(20.0f, nyquistGuard, stage.smoothedHz.getNextValue());
        stage.lowFilter.setCutoffFrequency(hz);
        stage.highFilter.setCutoffFrequency(hz);

        auto& band = bandOutputs[(size_t) s];
        band.setSize(numCh, numSamples, false, false, true);
        for (int ch = 0; ch < numCh; ++ch)
            band.copyFrom(ch, 0, remaining, ch, 0, numSamples);

        juce::dsp::AudioBlock<float> bandBlock(band);
        stage.lowFilter.process(juce::dsp::ProcessContextReplacing<float>(bandBlock));

        lowScratch.setSize(numCh, numSamples, false, false, true);
        for (int ch = 0; ch < numCh; ++ch)
            lowScratch.copyFrom(ch, 0, remaining, ch, 0, numSamples);

        juce::dsp::AudioBlock<float> highBlock(lowScratch);
        stage.highFilter.process(juce::dsp::ProcessContextReplacing<float>(highBlock));

        std::swap(remaining, lowScratch);
    }

    auto& lastBand = bandOutputs[(size_t) (numBands - 1)];
    lastBand.setSize(numCh, numSamples, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
        lastBand.copyFrom(ch, 0, remaining, ch, 0, numSamples);
}
