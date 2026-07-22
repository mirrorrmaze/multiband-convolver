#pragma once

#include <array>
#include <juce_core/juce_core.h>

namespace Params
{
    // Hard ceiling on band count. Chosen up front because JUCE's parameter model needs a fixed
    // set of AudioProcessorParameters registered at construction time (see ParameterLayout.cpp).
    // Bands beyond `numBands` (see below) are registered but inactive/unused.
    constexpr int maxBands = 8;
    constexpr int maxSplitPoints = maxBands - 1;

    // versionHint for all v1 parameters. Bump per-parameter when changing range/meaning in a way
    // that could break host automation/preset identity in a later version; never reuse a hint.
    constexpr int v1 = 1;

    // ---- Global parameters ----
    inline juce::String numBandsID()   { return "numBands"; }
    inline juce::String inputGainID()  { return "inputGain"; }
    inline juce::String outputGainID() { return "outputGain"; }

    inline juce::String splitHzID(int splitIndex)
    {
        jassert(splitIndex >= 0 && splitIndex < maxSplitPoints);
        return "splitHz_" + juce::String(splitIndex);
    }

    // ---- Per-band parameters ----
    inline juce::String bandIrIndexID(int band)   { return "band" + juce::String(band) + "_irIndex"; }
    inline juce::String bandDryWetID(int band)    { return "band" + juce::String(band) + "_dryWet"; }
    inline juce::String bandPreDelayID(int band)  { return "band" + juce::String(band) + "_preDelayMs"; }
    inline juce::String bandToneID(int band)      { return "band" + juce::String(band) + "_tone"; }
    inline juce::String bandFadeInID(int band)    { return "band" + juce::String(band) + "_fadeInMs"; }
    inline juce::String bandFadeOutID(int band)   { return "band" + juce::String(band) + "_fadeOut"; }
    inline juce::String bandStretchID(int band)   { return "band" + juce::String(band) + "_stretch"; }
    inline juce::String bandFeedbackID(int band)  { return "band" + juce::String(band) + "_feedback"; }
    inline juce::String bandOutGainID(int band)   { return "band" + juce::String(band) + "_outGain"; }
    inline juce::String bandBypassID(int band)    { return "band" + juce::String(band) + "_bypass"; }
    inline juce::String bandSoloID(int band)      { return "band" + juce::String(band) + "_solo"; }
    inline juce::String bandMuteID(int band)      { return "band" + juce::String(band) + "_mute"; }

    // Safety ceiling for the feedback parameter, enforced at the parameter range itself so host
    // automation cannot push the feedback loop past a safe gain (see architecture notes: feedback
    // tap still runs through a saturator + limiter downstream, this is belt-and-suspenders).
    constexpr float maxFeedbackPercent = 90.0f;
}
