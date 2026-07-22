#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeelSaturnish.h"

// A small hand-drawn floppy-disk glyph button (save preset) -- no external image asset needed.
class FloppyDiskButton final : public juce::Button
{
public:
    FloppyDiskButton() : juce::Button("Save Preset") {}

private:
    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(3.0f);

        auto colour = LookAndFeelSaturnish::text.withAlpha(isMouseOverButton ? 0.95f : 0.65f);
        if (isButtonDown)
            colour = LookAndFeelSaturnish::accent;
        g.setColour(colour);

        juce::Path body;
        body.addRoundedRectangle(bounds, 2.0f);
        g.strokePath(body, juce::PathStrokeType(1.4f));

        // Shutter: the metal slider strip near the top of a real floppy disk.
        auto shutter = bounds.removeFromTop(bounds.getHeight() * 0.4f).reduced(bounds.getWidth() * 0.22f, bounds.getHeight() * 0.28f);
        g.fillRoundedRectangle(shutter, 0.5f);

        // Label: the writable rectangle on the lower half.
        auto label = bounds.reduced(bounds.getWidth() * 0.16f, bounds.getHeight() * 0.12f);
        g.drawRoundedRectangle(label, 0.5f, 1.0f);
    }
};
