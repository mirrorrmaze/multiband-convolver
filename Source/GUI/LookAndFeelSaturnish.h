#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Dark, Saturn-2-ish theme: near-black panels, a small palette of saturated band colours, and a
// custom rotary knob (filled arc + pointer) instead of JUCE's default slider look.
class LookAndFeelSaturnish : public juce::LookAndFeel_V4
{
public:
    LookAndFeelSaturnish();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    static juce::Colour bandColour(int index);

    static const juce::Colour background;
    static const juce::Colour panel;
    static const juce::Colour text;
    static const juce::Colour accent;
};
