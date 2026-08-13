#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Params
{
    // Builds the full, fixed-slot parameter layout: global params + maxBands worth of per-band
    // params, all registered up front regardless of how many bands are currently active. See
    // Identifiers.h for the ID scheme and the architecture notes on why this is fixed-slot rather
    // than dynamically registered.
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Call before serializing apvts.state (host save, or PresetManager::savePreset) -- stamps
    // each band's currently-selected IR's relative path into the state tree alongside the
    // ordinary numeric bandN_irIndex parameter. See Identifiers.h's bandIrPathID comment for why.
    void stampSelectedIRPaths(juce::AudioProcessorValueTreeState& apvts);

    // Call after restoring apvts.state (host reload, or PresetManager::loadPreset) -- re-resolves
    // each band's bandN_irIndex from its stamped bandN_irPath (if present) against the *current*
    // IR catalog scan, so the same file is selected even if the Custom folder's contents (and
    // therefore the scan order) changed since the state was saved. A band with no stamped path
    // (state saved before this existed) or whose file no longer exists is left untouched, keeping
    // whatever numeric index was saved as a reasonable fallback.
    void resolveSelectedIRPaths(juce::AudioProcessorValueTreeState& apvts);

    // Copies every per-band setting (IR choice + its stamped path, Dry/Wet, Tone, Fade In/Out,
    // Stretch, Feedback, Pre-Delay, Output, Bypass/Solo/Mute) from one band slot to another.
    // Inserting or removing a band shifts which frequency range a given band *index* covers (see
    // SpectrumBandStrip::addBandAt/removeBand), so the settings have to move with it -- otherwise
    // a band index keeps whatever settings it had before, now applied to a different frequency
    // range than the one they were tuned for (reported as "sounds like the wrong frequency
    // group" / "the IR isn't on" after adding or removing bands).
    void copyBandSettings(juce::AudioProcessorValueTreeState& apvts, int fromBand, int toBand);
}
