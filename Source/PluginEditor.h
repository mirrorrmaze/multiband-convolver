#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GUI/SpectrumBandStrip.h"
#include "GUI/MacroPanelComponent.h"
#include "GUI/LookAndFeelSaturnish.h"
#include "GUI/KnobPopup.h"
#include "GUI/FloppyDiskButton.h"
#include "Params/PresetManager.h"

class MultibandConvolverAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit MultibandConvolverAudioProcessorEditor(MultibandConvolverAudioProcessor&);
    ~MultibandConvolverAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void wireHeaderKnob(juce::Slider& slider, const juce::String& unitSuffix);
    void refreshPresetBox(const juce::String& nameToSelect);
    void applyDefaultPreset();
    void promptAndSavePreset();
    void showHeaderMenu();
    void showAboutPopup();

    MultibandConvolverAudioProcessor& processorRef;
    PresetManager presetManager;

    LookAndFeelSaturnish lookAndFeel;
    juce::Label titleLabel;
    juce::TextButton resetButton { "Reset" };
    juce::TextButton menuButton { "..." };

    juce::ComboBox presetBox;
    FloppyDiskButton saveButton;

    juce::Slider inputGainKnob, outputGainKnob;
    juce::Label inputGainLabel, outputGainLabel;
    KnobPopup headerKnobPopup;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment, outputGainAttachment;

    SpectrumBandStrip bandStrip;
    MacroPanelComponent macroPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultibandConvolverAudioProcessorEditor)
};
