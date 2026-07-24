#include "MacroPanelComponent.h"
#include "LookAndFeelSaturnish.h"
#include "../Params/Identifiers.h"
#include "../DSP/IRLibrary.h"

namespace
{
    void styleKnob(juce::Slider& s)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0); // value shown via KnobPopup instead
    }
}

MacroPanelComponent::MacroPanelComponent(MultibandConvolverAudioProcessor& processorToUse)
    : processor(processorToUse)
{
    bandTitleLabel.setJustificationType(juce::Justification::centredLeft);
    bandTitleLabel.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)).withExtraKerningFactor(0.04f));
    addAndMakeVisible(bandTitleLabel);

    for (auto& entry : IRLibrary::getCatalog())
        irBox.addItem(entry.displayName, irBox.getNumItems() + 1);
    addAndMakeVisible(irBox);

    bypassButton.setClickingTogglesState(true);
    soloButton.setClickingTogglesState(true);
    muteButton.setClickingTogglesState(true);
    addAndMakeVisible(bypassButton);
    addAndMakeVisible(soloButton);
    addAndMakeVisible(muteButton);

    struct KnobSpec { juce::Slider* slider; juce::Label* label; const char* text; const char* unit; int decimals; };
    const KnobSpec specs[] = {
        { &preDelayKnob, &preDelayLabel, "Pre-Delay", " ms", 1 },
        { &toneKnob, &toneLabel, "Tone", "", 2 },
        { &fadeInKnob, &fadeInLabel, "Fade In", " ms", 1 },
        { &fadeOutKnob, &fadeOutLabel, "Fade Out", "%", 1 },
        { &stretchKnob, &stretchLabel, "Stretch", "x", 2 },
        { &feedbackKnob, &feedbackLabel, "Feedback", "%", 1 },
        { &dryWetKnob, &dryWetLabel, "Dry/Wet", "%", 1 },
        { &outGainKnob, &outGainLabel, "Output", " dB", 1 },
    };

    for (auto& spec : specs)
    {
        styleKnob(*spec.slider);
        addAndMakeVisible(*spec.slider);
        spec.label->setText(spec.text, juce::dontSendNotification);
        spec.label->setJustificationType(juce::Justification::centred);
        spec.label->setFont(juce::Font(juce::FontOptions(12.0f)).withExtraKerningFactor(0.03f));
        addAndMakeVisible(*spec.label);

        wireKnobPopup(*spec.slider, spec.unit, spec.decimals);
    }

    addChildComponent(knobPopup); // becomes visible on demand; added last so it paints on top

    setSelectedBand(0);
}

void MacroPanelComponent::wireKnobPopup(juce::Slider& slider, const juce::String& unitSuffix, int decimalPlaces)
{
    slider.onValueChange = [this, &slider, unitSuffix, decimalPlaces]
    {
        if (suppressPopup)
            return;

        auto* sliderPtr = &slider;
        knobPopup.showFor(slider, formatKnobText(slider, unitSuffix, decimalPlaces), slider.getValue(),
                           [sliderPtr] (double v) { sliderPtr->setValue(v, juce::sendNotificationSync); });
    };
}

juce::String MacroPanelComponent::formatKnobText(const juce::Slider& slider, const juce::String& unitSuffix, int decimalPlaces) const
{
    return juce::String(slider.getValue(), decimalPlaces) + unitSuffix;
}

void MacroPanelComponent::setSelectedBand(int bandIndex)
{
    if (bandIndex == currentBand)
        return;

    currentBand = bandIndex;
    bandTitleLabel.setText("Band " + juce::String(bandIndex + 1), juce::dontSendNotification);
    bandTitleLabel.setColour(juce::Label::textColourId, LookAndFeelSaturnish::bandColour(bandIndex));

    rebuildAttachments();
}

void MacroPanelComponent::rebuildAttachments()
{
    // Suppressed while attachments are (re)built -- construction synchronously pushes each
    // parameter's current value into its slider, which would otherwise pop the value bubble up
    // for every knob just from switching the selected band.
    suppressPopup = true;

    dryWetAttachment.reset();
    preDelayAttachment.reset();
    toneAttachment.reset();
    fadeInAttachment.reset();
    fadeOutAttachment.reset();
    stretchAttachment.reset();
    feedbackAttachment.reset();
    outGainAttachment.reset();
    irAttachment.reset();
    bypassAttachment.reset();
    soloAttachment.reset();
    muteAttachment.reset();

    auto& apvts = processor.apvts;
    const int b = currentBand;

    dryWetAttachment = std::make_unique<SliderAttachment>(apvts, Params::bandDryWetID(b), dryWetKnob);
    preDelayAttachment = std::make_unique<SliderAttachment>(apvts, Params::bandPreDelayID(b), preDelayKnob);
    toneAttachment = std::make_unique<SliderAttachment>(apvts, Params::bandToneID(b), toneKnob);
    fadeInAttachment = std::make_unique<SliderAttachment>(apvts, Params::bandFadeInID(b), fadeInKnob);
    fadeOutAttachment = std::make_unique<SliderAttachment>(apvts, Params::bandFadeOutID(b), fadeOutKnob);
    stretchAttachment = std::make_unique<SliderAttachment>(apvts, Params::bandStretchID(b), stretchKnob);
    feedbackAttachment = std::make_unique<SliderAttachment>(apvts, Params::bandFeedbackID(b), feedbackKnob);
    outGainAttachment = std::make_unique<SliderAttachment>(apvts, Params::bandOutGainID(b), outGainKnob);
    irAttachment = std::make_unique<ComboBoxAttachment>(apvts, Params::bandIrIndexID(b), irBox);
    bypassAttachment = std::make_unique<ButtonAttachment>(apvts, Params::bandBypassID(b), bypassButton);
    soloAttachment = std::make_unique<ButtonAttachment>(apvts, Params::bandSoloID(b), soloButton);
    muteAttachment = std::make_unique<ButtonAttachment>(apvts, Params::bandMuteID(b), muteButton);

    juce::Component::SafePointer<MacroPanelComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis]
    {
        if (safeThis != nullptr)
            safeThis->suppressPopup = false;
    });
}

void MacroPanelComponent::paint(juce::Graphics& g)
{
    if (auto* lf = dynamic_cast<LookAndFeelSaturnish*>(&getLookAndFeel()))
        lf->drawChassisPanel(g, getLocalBounds().toFloat(), true);
    else
        g.fillAll(LookAndFeelSaturnish::panel);

    g.setColour(LookAndFeelSaturnish::background);
    g.fillRect(getLocalBounds().removeFromTop(1));
}

void MacroPanelComponent::layoutKnob(juce::Rectangle<int> area, juce::Slider& slider, juce::Label& label)
{
    label.setBounds(area.removeFromTop(16));

    // Give the slider a tight, roughly-square box right under the label rather than all the
    // remaining column height -- otherwise its Component bounds extend far below the visible
    // knob (JUCE centres the drawn circle within whatever bounds it's given), which threw off
    // KnobPopup's "position below the knob" logic: getBottom() pointed way past the actual knob,
    // so the popup kept falling back to sitting above it, on top of the label.
    const int knobSize = juce::jmin(area.getWidth(), area.getHeight());
    slider.setBounds(area.removeFromTop(knobSize));
}

void MacroPanelComponent::resized()
{
    auto area = getLocalBounds().reduced(10);

    auto header = area.removeFromTop(28);
    bandTitleLabel.setBounds(header.removeFromLeft(120));
    muteButton.setBounds(header.removeFromRight(60).reduced(2));
    soloButton.setBounds(header.removeFromRight(60).reduced(2));
    bypassButton.setBounds(header.removeFromRight(70).reduced(2));
    irBox.setBounds(header.reduced(4, 0));

    area.removeFromTop(8);

    juce::Slider* knobs[] = { &preDelayKnob, &toneKnob, &fadeInKnob, &fadeOutKnob,
                              &stretchKnob, &feedbackKnob, &dryWetKnob, &outGainKnob };
    juce::Label* labels[] = { &preDelayLabel, &toneLabel, &fadeInLabel, &fadeOutLabel,
                              &stretchLabel, &feedbackLabel, &dryWetLabel, &outGainLabel };

    const int numKnobs = 8;
    const int knobWidth = area.getWidth() / numKnobs;
    for (int i = 0; i < numKnobs; ++i)
        layoutKnob(area.removeFromLeft(knobWidth).reduced(6), *knobs[i], *labels[i]);
}
