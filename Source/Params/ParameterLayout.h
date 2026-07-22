#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Params
{
    // Builds the full, fixed-slot parameter layout: global params + maxBands worth of per-band
    // params, all registered up front regardless of how many bands are currently active. See
    // Identifiers.h for the ID scheme and the architecture notes on why this is fixed-slot rather
    // than dynamically registered.
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
