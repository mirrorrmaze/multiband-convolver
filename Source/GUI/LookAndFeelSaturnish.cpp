#include "LookAndFeelSaturnish.h"
#include "BinaryData.h"

const juce::Colour LookAndFeelSaturnish::background { 0xff0b0a0a };
const juce::Colour LookAndFeelSaturnish::panel      { 0xff171514 };
const juce::Colour LookAndFeelSaturnish::text       { 0xffe9e2d0 };
const juce::Colour LookAndFeelSaturnish::accent     { 0xffd39a49 };

const juce::Colour LookAndFeelSaturnish::metalLight { 0xff4c4946 };
const juce::Colour LookAndFeelSaturnish::metalDark  { 0xff0c0b0a };
const juce::Colour LookAndFeelSaturnish::knurl      { 0xff1a1817 };

namespace
{
    const juce::Colour outline { 0xff09090a };
    const juce::Colour bypassOn { 0xffef9b4e };
    const juce::Colour soloOn   { 0xffe8d15c };
    const juce::Colour muteOn   { 0xffef6461 };

    void drawScrew(juce::Graphics& g, juce::Point<float> centre, float radius)
    {
        juce::ColourGradient grad(juce::Colour(0xff6a6866), centre.x - radius * 0.4f, centre.y - radius * 0.4f,
                                   juce::Colour(0xff0c0c0c), centre.x + radius * 0.5f, centre.y + radius * 0.5f, true);
        g.setGradientFill(grad);
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        const float angle = std::fmod(centre.x * 0.7f + centre.y * 1.3f, juce::MathConstants<float>::pi);
        juce::Line<float> slot(centre.x - radius * 0.7f * std::cos(angle), centre.y - radius * 0.7f * std::sin(angle),
                                centre.x + radius * 0.7f * std::cos(angle), centre.y + radius * 0.7f * std::sin(angle));
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.drawLine(slot, radius * 0.32f);
    }
}

LookAndFeelSaturnish::LookAndFeelSaturnish()
{
    knobMetalImage = juce::ImageCache::getFromMemory(BinaryData::knob_metal_jpg, BinaryData::knob_metal_jpgSize);
    chassisMetalImage = juce::ImageCache::getFromMemory(BinaryData::chassis_metal_jpg, BinaryData::chassis_metal_jpgSize);

    // Jost -- a free, open (SIL OFL) geometric sans modelled on the same 1920s German
    // grotesques (Kabel/Futura) that Moog used on the Minimoog's own control panel, rather than
    // the platform's default UI font.
    regularTypeface = juce::Typeface::createSystemTypefaceFor(BinaryData::JostRegular_ttf, BinaryData::JostRegular_ttfSize);
    semiBoldTypeface = juce::Typeface::createSystemTypefaceFor(BinaryData::JostSemiBold_ttf, BinaryData::JostSemiBold_ttfSize);

    setColour(juce::ResizableWindow::backgroundColourId, background);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, text.withAlpha(0.82f));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff201f1d));
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, outline);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff232120));
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha(0.3f));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff201f1d));
    setColour(juce::TextButton::textColourOffId, text.withAlpha(0.7f));
    setColour(juce::TextButton::textColourOnId, juce::Colours::black.withAlpha(0.85f));
    setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::CaretComponent::caretColourId, accent);
}

juce::Typeface::Ptr LookAndFeelSaturnish::getTypefaceForFont(const juce::Font& font)
{
    if (font.isBold() && semiBoldTypeface != nullptr)
        return semiBoldTypeface;

    if (regularTypeface != nullptr)
        return regularTypeface;

    return LookAndFeel_V4::getTypefaceForFont(font);
}

juce::Colour LookAndFeelSaturnish::bandColour(int index)
{
    static const juce::Colour palette[] = {
        juce::Colour(0xffbf5b52), juce::Colour(0xffc98a4a), juce::Colour(0xffc7ab54),
        juce::Colour(0xff7ea866), juce::Colour(0xff4f9ea8), juce::Colour(0xff7a76c2),
        juce::Colour(0xff9077b0), juce::Colour(0xff5fae97)
    };
    return palette[(size_t) (index % 8)];
}

void LookAndFeelSaturnish::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                            juce::Slider& slider)
{
    auto fullBounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(4.0f);
    const auto radius = juce::jmin(fullBounds.getWidth(), fullBounds.getHeight()) * 0.5f;
    const auto centre = fullBounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const float bodyRadius = radius * 0.84f; // leaves room for the tick-mark scale outside it

    // Drop shadow -- lifts the knob off the panel.
    {
        juce::Path circle;
        circle.addEllipse(centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
        juce::DropShadow shadow(juce::Colours::black.withAlpha(0.55f), (int) (bodyRadius * 0.35f),
                                 { (int) (bodyRadius * 0.12f), (int) (bodyRadius * 0.18f) });
        shadow.drawForPath(g, circle);
    }

    // Reference scale: short sunken/etched dashes around the knob's rotation range, marking
    // fixed points along the sweep (not knob-position-dependent) like the printed/engraved scale
    // around a real hardware knob. Each dash is a dark groove with a thin bright edge on one side,
    // same "etched into metal" language as the pointer, just shorter.
    {
        constexpr int numTicks = 11;
        const float tickInner = bodyRadius + radius * 0.05f;
        const float tickOuter = bodyRadius + radius * 0.14f;
        const float tickWidth = juce::jlimit(1.2f, 2.2f, radius * 0.045f);

        for (int i = 0; i < numTicks; ++i)
        {
            const float a = rotaryStartAngle + (float) i / (float) (numTicks - 1) * (rotaryEndAngle - rotaryStartAngle)
                             - juce::MathConstants<float>::halfPi;
            juce::Point<float> inner(centre.x + tickInner * std::cos(a), centre.y + tickInner * std::sin(a));
            juce::Point<float> outer(centre.x + tickOuter * std::cos(a), centre.y + tickOuter * std::sin(a));

            g.setColour(juce::Colours::black.withAlpha(0.8f));
            g.drawLine({ inner, outer }, tickWidth);

            const juce::Point<float> edgeOffset(tickWidth * 0.4f, tickWidth * 0.4f);
            g.setColour(juce::Colours::white.withAlpha(0.22f));
            g.drawLine({ inner + edgeOffset, outer + edgeOffset }, tickWidth * 0.4f);
        }
    }

    // Body: a real shiny brushed-steel photo texture (with a soft directional highlight baked
    // into the image itself, not drawn as a separate UI overlay) clipped to the knob circle. Every
    // knob uses the same orientation -- a real panel has one consistent light source, so rotating
    // the highlight per-knob would look wrong rather than more natural.
    {
        juce::Path bodyClip;
        bodyClip.addEllipse(centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
        g.saveState();
        g.reduceClipRegion(bodyClip);
        g.setOpacity(1.0f); // guard against any opacity left over from the drop-shadow draw above

        if (knobMetalImage.isValid())
        {
            const float imgW = (float) knobMetalImage.getWidth();
            const float imgH = (float) knobMetalImage.getHeight();
            const float coverScale = (bodyRadius * 2.0f) / juce::jmin(imgW, imgH);

            auto transform = juce::AffineTransform::scale(coverScale)
                                  .translated(centre.x - imgW * coverScale * 0.5f, centre.y - imgH * coverScale * 0.5f);
            g.drawImageTransformed(knobMetalImage, transform);
        }
        else
        {
            g.setColour(metalDark);
            g.fillEllipse(centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
        }

        // No extra code-drawn vignette here -- the texture now has its own baked-in highlight and
        // shading (see make_knob_texture.py), so a second mathematical gradient on top would just
        // fight it and look layered/digital again.
        g.restoreState();

        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.drawEllipse(centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.2f);
    }

    // Pointer: a fine engraved line -- thin dark groove plus an even thinner bright edge on one
    // side, close enough together to read as a scratch cut into the metal rather than two shapes.
    {
        const float pointerInner = bodyRadius * 0.2f;
        const float pointerOuter = bodyRadius * 0.82f;
        juce::Point<float> p1(centre.x + pointerInner * std::cos(angle - juce::MathConstants<float>::halfPi),
                               centre.y + pointerInner * std::sin(angle - juce::MathConstants<float>::halfPi));
        juce::Point<float> p2(centre.x + pointerOuter * std::cos(angle - juce::MathConstants<float>::halfPi),
                               centre.y + pointerOuter * std::sin(angle - juce::MathConstants<float>::halfPi));

        const float grooveWidth = juce::jlimit(1.6f, 3.2f, radius * 0.035f);
        g.setColour(juce::Colours::black.withAlpha(0.75f));
        g.drawLine({ p1, p2 }, grooveWidth);

        const juce::Point<float> edgeOffset(grooveWidth * 0.4f, grooveWidth * 0.5f);
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.drawLine({ p1 + edgeOffset, p2 + edgeOffset }, grooveWidth * 0.45f);
    }
}

void LookAndFeelSaturnish::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour&, bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool on = button.getToggleState();

    juce::Colour lit = accent;
    if (on)
    {
        const auto txt = button.getButtonText();
        if (txt == "Bypass")     lit = bypassOn;
        else if (txt == "Solo")  lit = soloOn;
        else if (txt == "Mute")  lit = muteOn;
    }

    g.setColour(juce::Colour(0xff0e0d0c));
    g.fillRoundedRectangle(bounds, 3.0f);

    auto face = bounds.reduced(2.2f);
    juce::Colour faceColour = on ? lit : juce::Colour(0xff2a2826);
    if (isDown)             faceColour = faceColour.darker(0.2f);
    else if (isHighlighted) faceColour = faceColour.brighter(on ? 0.08f : 0.15f);

    if (on)
    {
        juce::DropShadow glow(lit.withAlpha(0.55f), (int) (bounds.getHeight() * 0.6f), { 0, 0 });
        juce::Path facePath;
        facePath.addRoundedRectangle(face, 2.0f);
        glow.drawForPath(g, facePath);
    }

    juce::ColourGradient faceGradient(faceColour.brighter(on ? 0.15f : 0.06f), face.getTopLeft(),
                                      faceColour.darker(on ? 0.1f : 0.15f), face.getBottomLeft(), false);
    g.setGradientFill(faceGradient);
    g.fillRoundedRectangle(face, 2.0f);

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawRoundedRectangle(face, 2.0f, 1.0f);
}

void LookAndFeelSaturnish::drawChassisPanel(juce::Graphics& g, juce::Rectangle<float> bounds, bool withScrews) const
{
    g.saveState();
    g.reduceClipRegion(bounds.toNearestInt());

    if (chassisMetalImage.isValid())
        g.setTiledImageFill(chassisMetalImage, (int) bounds.getX(), (int) bounds.getY(), 1.0f);
    else
        g.setColour(panel);
    g.fillRect(bounds);

    // Darken the real texture substantially (it's a neutral-grey photo, the chassis needs to read
    // as near-black) and add a vignette so it isn't perfectly flat.
    g.setColour(juce::Colours::black.withAlpha(0.72f));
    g.fillRect(bounds);

    juce::ColourGradient vignette(juce::Colours::transparentBlack, bounds.getCentre(),
                                  juce::Colours::black.withAlpha(0.45f), bounds.getTopLeft(), true);
    g.setGradientFill(vignette);
    g.fillRect(bounds);

    g.restoreState();

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawRect(bounds, 1.0f);

    if (withScrews)
    {
        const float inset = 10.0f;
        const float r = 3.2f;
        drawScrew(g, { bounds.getX() + inset, bounds.getY() + inset }, r);
        drawScrew(g, { bounds.getRight() - inset, bounds.getY() + inset }, r);
        drawScrew(g, { bounds.getX() + inset, bounds.getBottom() - inset }, r);
        drawScrew(g, { bounds.getRight() - inset, bounds.getBottom() - inset }, r);
    }
}
