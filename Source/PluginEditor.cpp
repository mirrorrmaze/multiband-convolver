#include "PluginEditor.h"
#include "Params/Identifiers.h"
#include "DSP/IRLibrary.h"

namespace
{
    constexpr int headerHeight = 46; // tall enough for a small IN/OUT label above each master knob
    constexpr int defaultPresetId = 1;

    // Where "Skip This Version" persists so the update popup doesn't nag on every launch once
    // dismissed for a specific version -- a plain one-line text file, holding just the last
    // skipped version tag. A newer release than the skipped one still prompts -- this only
    // silences the exact version the user already said no to.
    juce::File getSkippedUpdateVersionFile()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MultibandConvolver");
        dir.createDirectory();
        return dir.getChildFile("skipped_update_version.txt");
    }

    juce::String getSkippedUpdateVersion()
    {
        return getSkippedUpdateVersionFile().loadFileAsString().trim();
    }

    void setSkippedUpdateVersion(const juce::String& version)
    {
        getSkippedUpdateVersionFile().replaceWithText(version);
    }
}

void MultibandConvolverAudioProcessorEditor::wireHeaderKnob(juce::Slider& slider, const juce::String& unitSuffix)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);

    slider.onValueChange = [this, &slider, unitSuffix]
    {
        auto* sliderPtr = &slider;
        headerKnobPopup.showFor(slider, juce::String(slider.getValue(), 1) + unitSuffix, slider.getValue(),
                                 [sliderPtr] (double v) { sliderPtr->setValue(v, juce::sendNotificationSync); });
    };
}

void MultibandConvolverAudioProcessorEditor::applyDefaultPreset()
{
    processorRef.resetAllParametersToDefault();
    bandStrip.resetSelectionToDefault();
}

void MultibandConvolverAudioProcessorEditor::refreshPresetBox(const juce::String& nameToSelect)
{
    presetBox.clear(juce::dontSendNotification);
    presetBox.addItem("Default", defaultPresetId);

    int id = defaultPresetId + 1;
    for (auto& name : presetManager.getUserPresetNames())
        presetBox.addItem(name, id++);

    for (int i = 0; i < presetBox.getNumItems(); ++i)
    {
        if (presetBox.getItemText(i) == nameToSelect)
        {
            presetBox.setSelectedItemIndex(i, juce::dontSendNotification);
            return;
        }
    }
    presetBox.setSelectedId(defaultPresetId, juce::dontSendNotification);
}

void MultibandConvolverAudioProcessorEditor::showHeaderMenu()
{
    juce::PopupMenu menu;

    if (availableUpdateVersion.isNotEmpty())
    {
        menu.addItem("Update available: " + availableUpdateVersion, [this]
        {
            availableUpdateUrl.launchInDefaultBrowser();
        });
        menu.addSeparator();
    }

    menu.addItem("Open Presets Folder", [this]
    {
        presetManager.getPresetsDirectory().revealToUser();
    });
    menu.addItem("Open IR Library Folder", []
    {
        IRLibrary::getCustomIRDirectory(); // ensures Custom/ exists so it's visible right away
        IRLibrary::resolveIRRoot().revealToUser();
    });
    menu.addItem("About", [this] { showAboutPopup(); });

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(menuButton));
}

void MultibandConvolverAudioProcessorEditor::showAboutPopup()
{
    juce::String text =
        "Multiband Convolver splits your signal into multiple frequency bands, each convolved "
        "independently against its own selectable room/plate/spring impulse response.\n\n"
        "The display up top shows each band's frequency range over a live spectrum of the "
        "incoming signal. Click a band to select it, double-click empty space to add a new "
        "band, right-click a band to remove it, and drag the vertical dividers to resize bands.\n\n"
        "The panel below shows the selected band's controls: pick its impulse response from the "
        "dropdown, then use Pre-Delay, Tone, Fade In/Out, Stretch, Feedback, Dry/Wet, and Output "
        "to shape it. Turn a knob to see its value in a popup; double-click that popup to type an "
        "exact number. Bypass/Solo/Mute affect just that band.\n\n"
        "Reset restores the default 3-band layout. The Presets menu saves/recalls your own full "
        "setups (band layout + every knob). IN/OUT in the top-right control overall input and "
        "output level.";

    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                            "About Multiband Convolver", text);
}

void MultibandConvolverAudioProcessorEditor::promptAndSavePreset()
{
    auto* aw = new juce::AlertWindow("Save Preset", "Enter a name for this preset:",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor("name", "", "Name:");
    aw->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw] (int result)
    {
        if (result == 1)
        {
            auto name = aw->getTextEditorContents("name").trim();
            if (name.isNotEmpty() && ! name.equalsIgnoreCase("Default"))
            {
                if (presetManager.savePreset(name))
                    refreshPresetBox(name);
            }
        }
    }), true);
}

MultibandConvolverAudioProcessorEditor::MultibandConvolverAudioProcessorEditor(MultibandConvolverAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), presetManager(p.apvts), bandStrip(p), macroPanel(p)
{
    setLookAndFeel(&lookAndFeel);

    titleLabel.setText("MULTIBAND CONVOLVER", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)).withExtraKerningFactor(0.06f));
    titleLabel.setColour(juce::Label::textColourId, LookAndFeelSaturnish::text.withAlpha(0.85f));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    menuButton.onClick = [this] { showHeaderMenu(); };
    addAndMakeVisible(menuButton);

    addAndMakeVisible(presetBox);
    refreshPresetBox("Default");
    presetBox.onChange = [this]
    {
        auto name = presetBox.getText();
        if (name == "Default")
            applyDefaultPreset();
        else if (presetManager.loadPreset(name))
            bandStrip.resetSelectionToDefault();
    };

    saveButton.onClick = [this] { promptAndSavePreset(); };
    addAndMakeVisible(saveButton);

    inputGainLabel.setText("IN", juce::dontSendNotification);
    outputGainLabel.setText("OUT", juce::dontSendNotification);
    for (auto* label : { &inputGainLabel, &outputGainLabel })
    {
        label->setJustificationType(juce::Justification::centred);
        label->setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        label->setColour(juce::Label::textColourId, LookAndFeelSaturnish::text.withAlpha(0.55f));
        addAndMakeVisible(*label);
    }

    wireHeaderKnob(inputGainKnob, " dB");
    wireHeaderKnob(outputGainKnob, " dB");
    addAndMakeVisible(inputGainKnob);
    addAndMakeVisible(outputGainKnob);

    inputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.apvts, Params::inputGainID(), inputGainKnob);
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.apvts, Params::outputGainID(), outputGainKnob);

    addChildComponent(headerKnobPopup);

    resetButton.onClick = [this] { applyDefaultPreset(); };
    addAndMakeVisible(resetButton);

    addAndMakeVisible(bandStrip);
    addAndMakeVisible(macroPanel);

    bandStrip.onBandSelected = [this] (int bandIndex)
    {
        macroPanel.setSelectedBand(bandIndex);
    };

    setResizable(true, true);
    setResizeLimits(700, 420, 1600, 1000);
    setSize(1000, 620);

    juce::Component::SafePointer<MultibandConvolverAudioProcessorEditor> safeThis(this);
    updateChecker.checkAsync(MULTIBAND_CONVOLVER_VERSION, [safeThis] (juce::String newVersion, juce::URL releaseUrl)
    {
        if (safeThis == nullptr)
            return;
        safeThis->availableUpdateVersion = newVersion;
        safeThis->availableUpdateUrl = releaseUrl;
        safeThis->menuButton.setColour(juce::TextButton::textColourOffId, LookAndFeelSaturnish::accent);

        // The passive indicator above always lights up; the popup below only interrupts once per
        // version -- "Skip This Version" persists so it won't ask again for this exact release (a
        // later one will still prompt), and just closing/escaping the dialog asks again next
        // launch rather than being remembered as a skip.
        if (newVersion == getSkippedUpdateVersion())
            return;

        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::InfoIcon)
                .withTitle("Update Available")
                .withMessage("Multiband Convolver " + newVersion + " is available -- you're running v"
                                 + juce::String(MULTIBAND_CONVOLVER_VERSION) + ".")
                .withButton("Download")
                .withButton("Skip This Version"),
            [safeThis, newVersion, releaseUrl] (int result)
            {
                if (safeThis == nullptr)
                    return;
                if (result == 1)
                    releaseUrl.launchInDefaultBrowser();
                else if (result == 2)
                    setSkippedUpdateVersion(newVersion);
            });
    });
}

MultibandConvolverAudioProcessorEditor::~MultibandConvolverAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void MultibandConvolverAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(LookAndFeelSaturnish::background);

    lookAndFeel.drawChassisPanel(g, getLocalBounds().removeFromTop(headerHeight).toFloat(), true);
}

void MultibandConvolverAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop(headerHeight);
    resetButton.setBounds(header.removeFromRight(76).reduced(10, 11));

    auto outputArea = header.removeFromRight(34).reduced(2, 2);
    outputGainLabel.setBounds(outputArea.removeFromTop(12));
    outputGainKnob.setBounds(outputArea.reduced(2, 0));

    auto inputArea = header.removeFromRight(34).reduced(2, 2);
    inputGainLabel.setBounds(inputArea.removeFromTop(12));
    inputGainKnob.setBounds(inputArea.reduced(2, 0));

    saveButton.setBounds(header.removeFromRight(28).reduced(3, 11));
    presetBox.setBounds(header.removeFromRight(140).reduced(4, 9));

    menuButton.setBounds(header.removeFromLeft(28).reduced(3, 11));

    titleLabel.setBounds(header);

    bandStrip.setBounds(area.removeFromTop(area.getHeight() * 2 / 5));
    macroPanel.setBounds(area);
}
