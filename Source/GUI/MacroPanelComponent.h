#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "KnobPopup.h"

// Lower-half macro knob rack. Shows the currently-selected band's controls; SliderAttachment /
// ComboBoxAttachment / ButtonAttachment objects are destroyed and rebuilt against
// `band{selectedIndex}_*` parameter IDs whenever the selection changes (cheap, per architecture
// notes -- attachments are lightweight to construct).
//
// Knob values show in a transient KnobPopup bubble (drag to show, holds briefly, fades out;
// double-click the bubble to type a value) rather than a persistent text box under each knob.
class MacroPanelComponent final : public juce::Component
{
public:
    explicit MacroPanelComponent(MultibandConvolverAudioProcessor& processorToUse);

    void resized() override;
    void paint(juce::Graphics&) override;

    void setSelectedBand(int bandIndex);

private:
    void rebuildAttachments();
    void layoutKnob(juce::Rectangle<int> area, juce::Slider& slider, juce::Label& label);
    void wireKnobPopup(juce::Slider& slider, const juce::String& unitSuffix, int decimalPlaces);
    juce::String formatKnobText(const juce::Slider& slider, const juce::String& unitSuffix, int decimalPlaces) const;

    MultibandConvolverAudioProcessor& processor;
    int currentBand = -1;
    bool suppressPopup = false; // true while attachments are being (re)constructed on band switch

    juce::Label bandTitleLabel;
    juce::ComboBox irBox;
    juce::TextButton bypassButton { "Bypass" }, soloButton { "Solo" }, muteButton { "Mute" };

    // Order matches the on-screen layout left-to-right; Dry/Wet sits just before Output per
    // user request.
    juce::Slider preDelayKnob, toneKnob, fadeInKnob, fadeOutKnob, stretchKnob, feedbackKnob, dryWetKnob, outGainKnob;
    juce::Label preDelayLabel, toneLabel, fadeInLabel, fadeOutLabel, stretchLabel, feedbackLabel, dryWetLabel, outGainLabel;

    KnobPopup knobPopup;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> dryWetAttachment, preDelayAttachment, toneAttachment,
        fadeInAttachment, fadeOutAttachment, stretchAttachment, feedbackAttachment, outGainAttachment;
    std::unique_ptr<ComboBoxAttachment> irAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment, soloAttachment, muteAttachment;
};
