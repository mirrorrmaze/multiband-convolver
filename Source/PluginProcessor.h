#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include "DSP/CrossoverSplitter.h"
#include "DSP/BandChain.h"
#include "DSP/IRReshapeWorker.h"
#include "Params/ParameterLayout.h"
#include "GUI/SpectrumAnalyzer.h"

class MultibandConvolverAudioProcessor final : public juce::AudioProcessor
{
public:
    MultibandConvolverAudioProcessor();
    ~MultibandConvolverAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Restores every parameter (band count, split points, all per-band macros) to its shipped
    // default -- used by the GUI's Reset button.
    void resetAllParametersToDefault();

    juce::AudioProcessorValueTreeState apvts;

    // Fed pre-split, post-input-gain audio; read by SpectrumBandStrip for the background analyzer.
    SpectrumAnalyzer inputAnalyzer;

    static constexpr int maxBands = Params::maxBands;

private:
    void updateBandMacrosFromParameters();

    // Set from the GUI thread by resetAllParametersToDefault(), consumed at the top of the next
    // processBlock() call. Actually resetting the DSP objects (as opposed to just their parameter
    // *values*, which is all the APVTS/host-automation path touches) has to happen on the audio
    // thread -- these are the same objects processBlock() is concurrently reading/writing.
    std::atomic<bool> pendingFullReset { false };

    juce::dsp::ConvolutionMessageQueue convolutionQueue;
    IRReshapeWorker reshapeWorker; // must construct before bandChains (their ctor takes a reference);
                                   // explicitly shut down in ~MultibandConvolverAudioProcessor() before
                                   // bandChains are destroyed, since it owns a background thread that
                                   // calls back into them.
    CrossoverSplitter splitter;
    std::array<std::unique_ptr<BandChain>, (size_t) maxBands> bandChains;
    std::array<juce::AudioBuffer<float>, (size_t) maxBands> bandBuffers;

    // Cached raw parameter pointers (lock-free atomics owned by the APVTS) for cheap per-block reads.
    std::atomic<float>* numBandsParam = nullptr;
    std::atomic<float>* inputGainParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::array<std::atomic<float>*, (size_t) Params::maxSplitPoints> splitHzParams {};

    struct BandParamPtrs
    {
        juce::AudioParameterChoice* irIndex = nullptr;
        std::atomic<float>* dryWet = nullptr;
        std::atomic<float>* preDelay = nullptr;
        std::atomic<float>* tone = nullptr;
        std::atomic<float>* fadeIn = nullptr;
        std::atomic<float>* fadeOut = nullptr;
        std::atomic<float>* stretch = nullptr;
        std::atomic<float>* feedback = nullptr;
        std::atomic<float>* outGain = nullptr;
        std::atomic<float>* bypass = nullptr;
        std::atomic<float>* solo = nullptr;
        std::atomic<float>* mute = nullptr;
    };
    std::array<BandParamPtrs, (size_t) maxBands> bandParams;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultibandConvolverAudioProcessor)
};
