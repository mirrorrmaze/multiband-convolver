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
}
