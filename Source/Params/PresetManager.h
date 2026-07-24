#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ParameterLayout.h"

// Simple file-backed preset store: each user preset is the APVTS state serialized to XML (the
// same format getStateInformation/setStateInformation already use for host save/reload), one file
// per preset under a per-user app-data folder. "Default" is deliberately NOT a file here -- it's
// a synthetic entry the GUI handles by calling resetAllParametersToDefault() directly, so it can
// never drift from whatever the shipped parameter defaults actually are.
class PresetManager
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& stateToUse) : apvts(stateToUse) {}

    juce::File getPresetsDirectory() const
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("Mirror Maze")
                       .getChildFile("Multiband Convolver")
                       .getChildFile("Presets");
        dir.createDirectory();
        return dir;
    }

    juce::StringArray getUserPresetNames() const
    {
        juce::StringArray names;
        for (const auto& f : getPresetsDirectory().findChildFiles(juce::File::findFiles, false, "*.xml"))
            names.add(f.getFileNameWithoutExtension());
        names.sort(true);
        return names;
    }

    // Returns false for an empty/reserved name or if writing the file failed.
    bool savePreset(const juce::String& name) const
    {
        if (name.trim().isEmpty() || name.trim().equalsIgnoreCase("Default"))
            return false;

        auto file = getPresetsDirectory().getChildFile(juce::File::createLegalFileName(name.trim()) + ".xml");
        Params::stampSelectedIRPaths(apvts);
        auto state = apvts.copyState();
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        return xml != nullptr && xml->writeTo(file);
    }

    bool loadPreset(const juce::String& name) const
    {
        auto file = getPresetsDirectory().getChildFile(juce::File::createLegalFileName(name) + ".xml");
        if (! file.existsAsFile())
            return false;

        std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
        if (xml == nullptr || ! xml->hasTagName(apvts.state.getType()))
            return false;

        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        Params::resolveSelectedIRPaths(apvts);
        return true;
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
};
