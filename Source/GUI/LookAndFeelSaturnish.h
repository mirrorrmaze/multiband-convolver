#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Hardware-emulation theme: dark scratched-metal chassis, photoreal-leaning knobs (radial metal
// gradient, knurled grip ring, drop shadow, specular highlight, pointer line) and illuminated
// bezel-style buttons, evoking vintage analog gear (SSL channel strips, old tape/rack units)
// rather than a flat plugin UI. All drawn procedurally (gradients/paths), no image assets.
class LookAndFeelSaturnish : public juce::LookAndFeel_V4
{
public:
    LookAndFeelSaturnish();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font&) override;

    // Draws the chassis panel background (vignette + brushed streaks + corner screws) into the
    // given bounds. Called explicitly by panel-owning components (not a LookAndFeel override).
    void drawChassisPanel(juce::Graphics&, juce::Rectangle<float> bounds, bool withScrews = true) const;

    static juce::Colour bandColour(int index);

    static const juce::Colour background;
    static const juce::Colour panel;
    static const juce::Colour text;
    static const juce::Colour accent;

    static const juce::Colour metalLight;
    static const juce::Colour metalDark;
    static const juce::Colour knurl;

private:
    juce::Image knobMetalImage;
    juce::Image chassisMetalImage;

    juce::Typeface::Ptr regularTypeface;
    juce::Typeface::Ptr semiBoldTypeface;
};
