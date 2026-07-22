#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace IRLibrary
{
    struct Entry
    {
        juce::String displayName;   // shown in the IR picker
        juce::String category;      // Residential / Commercial / Public / Historical / Outdoors / Textures
        juce::String relativePath;  // relative to the resolved IR root, e.g. "Residential/Arroyo House Living Room Close A.wav"
    };

    // The curated starter library: 46 MIT-licensed IRs cherry-picked from itsmusician/IR-Library
    // (see Resources/IRs/CREDITS.md). Order here is the order shown in the IR picker and defines
    // the integer index used by the band{N}_irIndex parameter -- do not reorder after shipping a
    // version with saved presets/automation; append new entries at the end instead.
    const std::vector<Entry>& getCatalog();

    // Resolves the root folder that contains the category subfolders (Residential/, Commercial/,
    // etc.). Searches, in order: next to the running plugin/standalone binary, then the source
    // tree's Resources/IRs (dev-build fallback), then a user Documents folder (for a future
    // installer). Returns an invalid File if none of those exist.
    juce::File resolveIRRoot();

    // Loads entry `index`'s audio into `outBuffer` at `targetSampleRate` (resampled if needed).
    // Returns false (and leaves outBuffer empty) if the file can't be found or read.
    bool loadEntry(int index, double targetSampleRate, juce::AudioBuffer<float>& outBuffer);

    constexpr int catalogSize = 46;
}
