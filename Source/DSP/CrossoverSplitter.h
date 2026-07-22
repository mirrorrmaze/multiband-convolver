#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include "../Params/Identifiers.h"

// Splits a signal into up to Params::maxBands contiguous bands using a cascaded Linkwitz-Riley
// (LR4, 24 dB/oct) crossover ladder driven by a sorted array of shared split-point frequencies.
//
// Topology: split point s takes `remaining` (the not-yet-band-assigned signal) and produces
// band[s] = lowpass(remaining, splitHz[s]) and a new `remaining` = highpass(remaining, splitHz[s]).
// The last band (index numBands-1) is whatever remains after the final split -- no filter needed.
// With all bands recombined (sum), this reconstructs the input near-exactly (LR crossovers sum to
// unity-gain allpass) -- see the Phase 2 null test. Once each band goes through independent
// convolution downstream, that reconstruction property no longer holds (by design), but it's what
// keeps the *splitting* itself transparent and click-free while bands are added/resized live.
class CrossoverSplitter
{
public:
    CrossoverSplitter();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // Called once per block before process(). numBands in [1, Params::maxBands].
    void setNumBands(int numBands);

    // Target frequency for split point `index` (0 .. numBands-2). Ramped internally over ~50ms
    // to avoid zipper noise when a user drags a band edge.
    void setSplitHz(int index, float hz);

    // Splits `input` (numChannels x numSamples) into `bandOutputs[0..numBands-1]`, each resized to
    // match input's channel/sample count. Buffers beyond the active band count are left untouched.
    void process(const juce::AudioBuffer<float>& input,
                 std::array<juce::AudioBuffer<float>, (size_t) Params::maxBands>& bandOutputs);

    int getNumBands() const noexcept { return numBands; }

private:
    struct SplitStage
    {
        juce::dsp::LinkwitzRileyFilter<float> lowFilter;
        juce::dsp::LinkwitzRileyFilter<float> highFilter;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedHz { 1000.0f };
        bool needsReset = true;
    };

    std::array<SplitStage, (size_t) Params::maxSplitPoints> stages;
    juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
    int numBands = 1;

    // Scratch buffers reused across process() calls to avoid per-block allocation.
    juce::AudioBuffer<float> remaining;
    juce::AudioBuffer<float> lowScratch;
};
