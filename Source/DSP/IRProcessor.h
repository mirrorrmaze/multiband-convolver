#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Pure, stateless IR shaping: loads a catalog entry and applies stretch (resample) + fade in/out.
// Deliberately free of any shared state so it's safe to call from a background thread -- see
// IRReshapeWorker, which is what actually calls this off the audio thread. This is exactly the
// work that used to run inline inside BandChain::process() and caused real CPU spikes/dropouts
// when dragging Stretch or picking a new IR (confirmed via a user-supplied recording showing
// Ableton's CPU meter jumping past 100% at the moment of a Stretch drag).
namespace IRProcessor
{
    // Returns false (leaving outShapedIR empty) if the catalog entry couldn't be loaded.
    bool buildShapedIR(int irIndex, double sampleRate, float fadeInMs, float fadeOutPercent, float stretch,
                        juce::AudioBuffer<float>& outShapedIR);

    // The fade-in ramp length and fade-out keep-length (in samples) for a buffer of
    // `naturalLength` samples. Factored out of buildShapedIR so IRWaveformView's overlay can
    // compute the exact same region without duplicating (and risking drifting from) this math.
    void computeFadeRegion(int naturalLength, double sampleRate, float fadeInMs, float fadeOutPercent,
                            int& outFadeInSamples, int& outKeptLength);
}
