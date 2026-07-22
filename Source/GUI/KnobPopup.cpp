#include "KnobPopup.h"
#include "LookAndFeelSaturnish.h"

KnobPopup::KnobPopup()
{
    setInterceptsMouseClicks(false, false); // only the editor (added on demand) needs clicks
    setAlwaysOnTop(true);
    setVisible(false);

    editor.addListener(this);
    editor.setJustification(juce::Justification::centred);
    editor.setInputRestrictions(10, "-.0123456789");
    editor.setSelectAllWhenFocused(true);
    editor.setFont(juce::Font(juce::FontOptions(13.0f)));
    addChildComponent(editor);
}

void KnobPopup::showFor(juce::Component& target, const juce::String& text,
                         double currentValue, std::function<void(double)> onValueEntered)
{
    if (editing)
        return; // don't yank the bubble out from under an in-progress manual edit

    displayText = text;
    rawValue = currentValue;
    valueCallback = std::move(onValueEntered);

    const int w = 74, h = 22;
    auto targetBounds = target.getBounds(); // target shares this component's parent coordinate space
    int x = targetBounds.getCentreX() - w / 2;

    // Below the knob rather than above -- above collides with the name label that sits directly
    // on top of each knob (Dry/Wet, Feedback, etc.).
    int y = targetBounds.getBottom() + 4;
    if (auto* parent = getParentComponent())
        if (y + h > parent->getHeight())
            y = targetBounds.getY() - h - 4; // fall back to above only if there's truly no room below

    setBounds(x, y, w, h);
    setInterceptsMouseClicks(true, true);
    setVisible(true);
    toFront(false);

    holdTicksRemaining = (holdMs * tickHz) / 1000;
    fadeTicksRemaining = (fadeMs * tickHz) / 1000;
    alpha = 1.0f;
    startTimerHz(tickHz);
    repaint();
}

void KnobPopup::timerCallback()
{
    if (editing)
        return;

    if (holdTicksRemaining > 0)
    {
        --holdTicksRemaining;
        return;
    }

    const int totalFadeTicks = (fadeMs * tickHz) / 1000;
    if (fadeTicksRemaining > 0)
    {
        --fadeTicksRemaining;
        alpha = totalFadeTicks > 0 ? (float) fadeTicksRemaining / (float) totalFadeTicks : 0.0f;
        repaint();
    }
    else
    {
        setVisible(false);
        setInterceptsMouseClicks(false, false);
        stopTimer();
    }
}

void KnobPopup::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    if (editing)
    {
        g.setColour(LookAndFeelSaturnish::panel.brighter(0.1f));
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(LookAndFeelSaturnish::accent);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.2f);
        return;
    }

    const float shown = alpha * maxAlpha;
    g.setColour(LookAndFeelSaturnish::panel.darker(0.1f).withAlpha(shown));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(LookAndFeelSaturnish::accent.withAlpha(shown));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);

    g.setColour(LookAndFeelSaturnish::text.withAlpha(shown));
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.drawText(displayText, getLocalBounds(), juce::Justification::centred);
}

void KnobPopup::mouseDoubleClick(const juce::MouseEvent&)
{
    beginEdit();
}

void KnobPopup::beginEdit()
{
    editing = true;
    holdTicksRemaining = 1 << 20; // frozen while editing
    alpha = 1.0f;

    editor.setText(juce::String(rawValue, 3), juce::dontSendNotification);
    editor.setBounds(getLocalBounds());
    editor.setVisible(true);
    editor.toFront(true);
    editor.grabKeyboardFocus();
    editor.selectAll();
    repaint();
}

void KnobPopup::endEdit(bool commit)
{
    if (! editing)
        return;

    editing = false;
    editor.setVisible(false);

    if (commit)
    {
        const double parsed = editor.getText().getDoubleValue();
        if (valueCallback)
            valueCallback(parsed);
    }

    // Restart the normal hold-then-fade cycle so the bubble doesn't just vanish abruptly.
    holdTicksRemaining = (holdMs * tickHz) / 1000;
    fadeTicksRemaining = (fadeMs * tickHz) / 1000;
    alpha = 1.0f;
    startTimerHz(tickHz);
    repaint();
}

void KnobPopup::textEditorReturnKeyPressed(juce::TextEditor&) { endEdit(true); }
void KnobPopup::textEditorEscapeKeyPressed(juce::TextEditor&) { endEdit(false); }
void KnobPopup::textEditorFocusLost(juce::TextEditor&) { endEdit(false); }
