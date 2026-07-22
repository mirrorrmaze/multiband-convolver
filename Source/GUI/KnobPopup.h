#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// A small value bubble that appears near a knob while it's being dragged, stays for a moment
// after release, then fades out. Double-clicking it while visible switches to a text editor so
// the value can be typed directly; Enter commits, Escape/focus-loss cancels.
class KnobPopup final : public juce::Component,
                         private juce::Timer,
                         private juce::TextEditor::Listener
{
public:
    KnobPopup();

    // Positions the bubble above `target` (must share this component's parent), shows `text`,
    // and (re)starts the hold-then-fade countdown. `currentValue`/`onValueEntered` back the
    // type-to-set editing gesture.
    void showFor(juce::Component& target, const juce::String& text,
                 double currentValue, std::function<void(double)> onValueEntered);

    void paint(juce::Graphics&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void beginEdit();
    void endEdit(bool commit);

    void textEditorReturnKeyPressed(juce::TextEditor&) override;
    void textEditorEscapeKeyPressed(juce::TextEditor&) override;
    void textEditorFocusLost(juce::TextEditor&) override;

    juce::String displayText;
    double rawValue = 0.0;
    std::function<void(double)> valueCallback;

    juce::TextEditor editor;
    bool editing = false;

    float alpha = 1.0f;
    int holdTicksRemaining = 0;
    int fadeTicksRemaining = 0;

    static constexpr int tickHz = 30;
    static constexpr int holdMs = 550;
    static constexpr int fadeMs = 200;
    static constexpr float maxAlpha = 0.72f; // capped so the bubble never fully occludes the knob
};
