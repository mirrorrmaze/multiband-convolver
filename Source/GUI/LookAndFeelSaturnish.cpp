#include "LookAndFeelSaturnish.h"

const juce::Colour LookAndFeelSaturnish::background { 0xff1a1b1f };
const juce::Colour LookAndFeelSaturnish::panel      { 0xff232529 };
const juce::Colour LookAndFeelSaturnish::text       { 0xffecedf1 };
const juce::Colour LookAndFeelSaturnish::accent     { 0xff4fd8e0 };

namespace
{
    const juce::Colour outline { 0xff34363c };
    const juce::Colour bypassOn { 0xffef9b4e };
    const juce::Colour soloOn   { 0xffe8d15c };
    const juce::Colour muteOn   { 0xffef6461 };
}

LookAndFeelSaturnish::LookAndFeelSaturnish()
{
    setColour(juce::ResizableWindow::backgroundColourId, background);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, text.withAlpha(0.8f));
    setColour(juce::ComboBox::backgroundColourId, panel);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, outline);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha(0.25f));
    setColour(juce::TextButton::buttonColourId, panel);
    setColour(juce::TextButton::textColourOffId, text.withAlpha(0.7f));
    setColour(juce::TextButton::textColourOnId, juce::Colours::black.withAlpha(0.85f));
    setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::CaretComponent::caretColourId, accent);
}

juce::Colour LookAndFeelSaturnish::bandColour(int index)
{
    static const juce::Colour palette[] = {
        juce::Colour(0xffef6461), juce::Colour(0xfff2a154), juce::Colour(0xffe8d15c),
        juce::Colour(0xff8fce6b), juce::Colour(0xff4fd8e0), juce::Colour(0xff8a7ff0),
        juce::Colour(0xffe07be0), juce::Colour(0xff6fe0c5)
    };
    return palette[(size_t) (index % 8)];
}

void LookAndFeelSaturnish::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                            juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(4.0f);
    const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const float trackWidth = juce::jmax(2.0f, radius * 0.16f);

    // Background track.
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius - trackWidth, radius - trackWidth,
                        0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(panel.brighter(0.12f));
    g.strokePath(track, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Small flat tick marks at 0/25/50/75/100% -- a clean, modern touch rather than a busy dial.
    g.setColour(outline.brighter(0.2f));
    for (int i = 0; i <= 4; ++i)
    {
        const float t = (float) i / 4.0f;
        const float tickAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const float inner = radius + 1.5f;
        const float outer = radius + 4.5f;
        juce::Point<float> p1(centre.x + inner * std::cos(tickAngle - juce::MathConstants<float>::halfPi),
                               centre.y + inner * std::sin(tickAngle - juce::MathConstants<float>::halfPi));
        juce::Point<float> p2(centre.x + outer * std::cos(tickAngle - juce::MathConstants<float>::halfPi),
                               centre.y + outer * std::sin(tickAngle - juce::MathConstants<float>::halfPi));
        g.drawLine({ p1, p2 }, 1.2f);
    }

    // Value arc.
    juce::Path valueArc;
    valueArc.addCentredArc(centre.x, centre.y, radius - trackWidth, radius - trackWidth,
                           0.0f, rotaryStartAngle, angle, true);
    auto fillColour = slider.findColour(juce::Slider::rotarySliderFillColourId, true);
    g.setColour(fillColour.getAlpha() > 0 ? fillColour : accent);
    g.strokePath(valueArc, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Flat puck with a thin rim and a subtle top-lit gradient for a touch of depth without
    // looking skeuomorphic.
    auto puckBounds = juce::Rectangle<float>(centre.x - radius * 0.72f, centre.y - radius * 0.72f,
                                              radius * 1.44f, radius * 1.44f);
    juce::ColourGradient puckGradient(panel.brighter(0.06f), puckBounds.getTopLeft(),
                                       panel.darker(0.15f), puckBounds.getBottomLeft(), false);
    g.setGradientFill(puckGradient);
    g.fillEllipse(puckBounds);
    g.setColour(outline);
    g.drawEllipse(puckBounds, 1.0f);

    // Short rim-mounted indicator stub rather than a full centre-to-edge pointer.
    const float stubInner = radius * 0.5f;
    const float stubOuter = radius * 0.72f;
    juce::Point<float> stubStart(centre.x + stubInner * std::cos(angle - juce::MathConstants<float>::halfPi),
                                  centre.y + stubInner * std::sin(angle - juce::MathConstants<float>::halfPi));
    juce::Point<float> stubEnd(centre.x + stubOuter * std::cos(angle - juce::MathConstants<float>::halfPi),
                                centre.y + stubOuter * std::sin(angle - juce::MathConstants<float>::halfPi));
    g.setColour(text);
    g.drawLine({ stubStart, stubEnd }, 2.2f);
}

void LookAndFeelSaturnish::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour&, bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool on = button.getToggleState();

    juce::Colour base = panel;
    if (on)
    {
        const auto txt = button.getButtonText();
        if (txt == "Bypass")     base = bypassOn;
        else if (txt == "Solo")  base = soloOn;
        else if (txt == "Mute")  base = muteOn;
        else                     base = accent;
    }

    if (isDown)             base = base.darker(0.15f);
    else if (isHighlighted) base = on ? base.brighter(0.08f) : base.brighter(0.15f);

    g.setColour(base);
    g.fillRoundedRectangle(bounds, 5.0f);

    if (! on)
    {
        g.setColour(outline);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);
    }
}
