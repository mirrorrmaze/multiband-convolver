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

    // Grouped by category with bold section headings (Residential/Commercial/Public/Historical/
    // Outdoors/Textures/Custom), matching the picker layout in the sibling GGrid project's own
    // convolver. Item IDs are still just catalogIndex + 1 regardless of how many headings get
    // interspersed -- addSectionHeading() doesn't consume an ID, so this can't disturb the
    // catalog's permanently-stable indices (see IRLibrary.h) that saved presets/automation rely
    // on. A category can legitimately appear as more than one separate heading (the catalog's
    // factory entries were appended in batches over time, not fully re-sorted by category), which
    // is fine -- it's just a display grouping, not a reordering.
    juce::String lastCategory;
    const auto& catalog = IRLibrary::getCatalog();
    for (int i = 0; i < (int) catalog.size(); ++i)
    {
        const auto& entry = catalog[(size_t) i];
        if (entry.category != lastCategory)
        {
            irBox.addSectionHeading(entry.category);
            lastCategory = entry.category;
        }
        irBox.addItem(entry.displayName, i + 1);
    }
    irBox.onChange = [this] { updateWaveformDisplay(); };
    addAndMakeVisible(irBox);

    addAndMakeVisible(waveformView);

    bypassButton.setClickingTogglesState(true);
    soloButton.setClickingTogglesState(true);
    muteButton.setClickingTogglesState(true);
    addAndMakeVisible(bypassButton);
    addAndMakeVisible(soloButton);
    addAndMakeVisible(muteButton);

    struct KnobSpec { juce::Slider* slider; juce::Label* label; const char* text; const char* unit; int decimals; bool affectsWaveform; };
    const KnobSpec specs[] = {
        { &preDelayKnob, &preDelayLabel, "Pre-Delay", " ms", 1, false },
        { &toneKnob, &toneLabel, "Tone", "", 2, false },
        { &fadeInKnob, &fadeInLabel, "Fade In", " ms", 1, true },
        { &fadeOutKnob, &fadeOutLabel, "Fade Out", "%", 1, true },
        { &stretchKnob, &stretchLabel, "Stretch", "x", 2, true },
        { &feedbackKnob, &feedbackLabel, "Feedback", "%", 1, false },
        { &dryWetKnob, &dryWetLabel, "Dry/Wet", "%", 1, false },
        { &outGainKnob, &outGainLabel, "Output", " dB", 1, false },
    };

    for (auto& spec : specs)
    {
        styleKnob(*spec.slider);
        addAndMakeVisible(*spec.slider);
        spec.label->setText(spec.text, juce::dontSendNotification);
        spec.label->setJustificationType(juce::Justification::centred);
        spec.label->setFont(juce::Font(juce::FontOptions(12.0f)).withExtraKerningFactor(0.03f));
        addAndMakeVisible(*spec.label);

        wireKnobPopup(*spec.slider, spec.unit, spec.decimals, spec.affectsWaveform);
    }

    addChildComponent(knobPopup); // becomes visible on demand; added last so it paints on top

    setSelectedBand(0);
}

void MacroPanelComponent::wireKnobPopup(juce::Slider& slider, const juce::String& unitSuffix, int decimalPlaces, bool affectsWaveform)
{
    slider.onValueChange = [this, &slider, unitSuffix, decimalPlaces, affectsWaveform]
    {
        // Runs even while suppressPopup is true (band switch) -- the waveform should still track
        // the newly-selected band's values, it's just the popup bubble that shouldn't appear.
        if (affectsWaveform)
            updateWaveformDisplay();

        if (suppressPopup)
            return;

        auto* sliderPtr = &slider;
        knobPopup.showFor(slider, formatKnobText(slider, unitSuffix, decimalPlaces), slider.getValue(),
                           [sliderPtr] (double v) { sliderPtr->setValue(v, juce::sendNotificationSync); });
    };
}

void MacroPanelComponent::updateWaveformDisplay()
{
    const int irIndex = irBox.getSelectedId() - 1; // ids are 1-based, matching catalog index + 1
    if (irIndex < 0)
        return;

    waveformView.refresh(irIndex, processor.getSampleRate(), (float) fadeInKnob.getValue(),
                          (float) fadeOutKnob.getValue(), (float) stretchKnob.getValue());
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

    updateWaveformDisplay(); // don't rely on attachment construction firing onValueChange
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

    // Proportional (not a fixed pixel height) so the waveform strip and the knob row below it
    // scale together across the plugin's resizable range (700x420 - 1600x1000) instead of the
    // strip eating a fixed chunk that would crush the knobs at the smallest window size.
    auto waveformArea = area.removeFromTop(juce::jlimit(50, 130, area.getHeight() * 3 / 10));
    waveformView.setBounds(waveformArea.reduced(4, 0));

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
