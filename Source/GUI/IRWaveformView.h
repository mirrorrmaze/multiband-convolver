#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>

// Shows the currently-selected band's impulse response -- the exact shaped buffer (post-Stretch,
// post-Fade In/Out) that gets loaded into the convolution engine, so Fade In/Out's effect on the
// envelope and Stretch's effect on density/length are all visible directly on the waveform rather
// than only audible after the fact.
//
// Deliberately decoupled from the real-time reload pipeline (BandChain/IRReshapeWorker): this is
// a second, GUI-only load+shape path so a busy display never has any chance of competing with or
// blocking the audio-thread-facing one. It reuses IRProcessor::buildShapedIR() directly -- the
// same pure, stateless shaping function the real engine uses, so what's drawn always matches what
// plays -- on its own background thread, debounced the same way BandChain debounces reloads (to
// avoid kicking off a real disk-read-and-resample on every pixel of a knob drag), just via a
// wall-clock juce::Timer instead of a sample-count countdown since this lives on the message
// thread already.
class IRWaveformView final : public juce::Component,
                              private juce::Thread,
                              private juce::AsyncUpdater,
                              private juce::Timer
{
public:
    IRWaveformView();
    ~IRWaveformView() override;

    // Cheap to call on every knob tick -- only the debounce timer restarts until it settles, so
    // rapid dragging never triggers more than one actual reshape.
    void refresh(int irIndex, double sampleRate, float fadeInMs, float fadeOutPercent, float stretch);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void run() override;              // background thread: shape the IR via IRProcessor
    void handleAsyncUpdate() override; // message thread: adopt the new buffer, rebuild display points
    void timerCallback() override;     // message thread: debounce settled, hand the job to the worker

    void rebuildDisplayPoints(); // downsamples displayedBuffer into per-column min/max at current width

    struct Job
    {
        int irIndex = -1;
        double sampleRate = 44100.0;
        float fadeInMs = 0.0f;
        float fadeOutPercent = 0.0f;
        float stretch = 1.0f;
    };

    Job pendingRefreshJob; // message thread only, latest request from refresh()

    juce::SpinLock jobLock;
    Job requestedJob;
    bool jobPending = false;

    juce::SpinLock resultLock;
    juce::AudioBuffer<float> pendingResultBuffer;
    double pendingResultSampleRate = 44100.0;
    bool resultPending = false;

    juce::AudioBuffer<float> displayedBuffer; // message-thread-owned
    double displayedSampleRate = 44100.0;
    std::vector<float> minPoints, maxPoints;  // one pair per pixel column of the last-laid-out width

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IRWaveformView)
};
