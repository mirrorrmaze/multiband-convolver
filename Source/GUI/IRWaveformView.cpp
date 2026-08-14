#include "IRWaveformView.h"
#include "LookAndFeelSaturnish.h"
#include "../DSP/IRProcessor.h"

IRWaveformView::IRWaveformView() : juce::Thread("IR Waveform")
{
    startThread(juce::Thread::Priority::background);
}

IRWaveformView::~IRWaveformView()
{
    stopTimer();
    stopThread(3000);
    cancelPendingUpdate();
}

void IRWaveformView::refresh(int irIndex, double sampleRate, float fadeInMs, float fadeOutPercent, float stretch)
{
    pendingRefreshJob = { irIndex, sampleRate, fadeInMs, fadeOutPercent, stretch };
    startTimer(120); // restarts on every call -- fires once ~120ms after the last change settles
}

void IRWaveformView::timerCallback()
{
    stopTimer();

    {
        const juce::SpinLock::ScopedLockType lock(jobLock);
        requestedJob = pendingRefreshJob;
        jobPending = true;
    }
    notify();
}

void IRWaveformView::run()
{
    while (! threadShouldExit())
    {
        Job job;
        bool haveJob = false;
        {
            const juce::SpinLock::ScopedLockType lock(jobLock);
            if (jobPending) { job = requestedJob; jobPending = false; haveJob = true; }
        }

        if (! haveJob)
        {
            wait(50);
            continue;
        }

        juce::AudioBuffer<float> shaped;
        if (IRProcessor::buildShapedIR(job.irIndex, job.sampleRate, job.fadeInMs, job.fadeOutPercent, job.stretch, shaped))
        {
            const juce::SpinLock::ScopedLockType lock(resultLock);
            pendingResultBuffer = std::move(shaped);
            pendingResultSampleRate = job.sampleRate;
            resultPending = true;
        }

        if (threadShouldExit())
            return;

        triggerAsyncUpdate();
    }
}

void IRWaveformView::handleAsyncUpdate()
{
    juce::AudioBuffer<float> buf;
    double sr = displayedSampleRate;
    {
        const juce::SpinLock::ScopedLockType lock(resultLock);
        if (! resultPending)
            return;
        buf = std::move(pendingResultBuffer);
        sr = pendingResultSampleRate;
        resultPending = false;
    }

    displayedBuffer = std::move(buf);
    displayedSampleRate = sr;
    rebuildDisplayPoints();
    repaint();
}

void IRWaveformView::rebuildDisplayPoints()
{
    const int width = getWidth();
    minPoints.assign((size_t) juce::jmax(0, width), 0.0f);
    maxPoints.assign((size_t) juce::jmax(0, width), 0.0f);

    const int numSamples = displayedBuffer.getNumSamples();
    const int numChannels = displayedBuffer.getNumChannels();
    if (width <= 0 || numSamples <= 0 || numChannels <= 0)
        return;

    for (int x = 0; x < width; ++x)
    {
        const int startSample = (int) ((juce::int64) x * numSamples / width);
        const int endSample = juce::jmax(startSample + 1, (int) ((juce::int64) (x + 1) * numSamples / width));

        float lo = 0.0f, hi = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto range = displayedBuffer.findMinMax(ch, startSample, endSample - startSample);
            lo = juce::jmin(lo, range.getStart());
            hi = juce::jmax(hi, range.getEnd());
        }
        minPoints[(size_t) x] = lo;
        maxPoints[(size_t) x] = hi;
    }
}

void IRWaveformView::resized()
{
    rebuildDisplayPoints();
}

void IRWaveformView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(LookAndFeelSaturnish::background);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(LookAndFeelSaturnish::metalLight.withAlpha(0.5f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

    if (minPoints.empty())
    {
        g.setColour(LookAndFeelSaturnish::text.withAlpha(0.35f));
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText("Loading IR...", bounds, juce::Justification::centred);
        return;
    }

    const float midY = bounds.getCentreY();
    const float halfH = bounds.getHeight() * 0.5f - 4.0f;

    g.setColour(LookAndFeelSaturnish::metalDark.withAlpha(0.6f));
    g.drawHorizontalLine((int) midY, bounds.getX(), bounds.getRight());

    juce::Path waveform;
    const int width = (int) minPoints.size();
    waveform.startNewSubPath(0.0f, midY - maxPoints[0] * halfH);
    for (int x = 1; x < width; ++x)
        waveform.lineTo((float) x, midY - maxPoints[(size_t) x] * halfH);
    for (int x = width - 1; x >= 0; --x)
        waveform.lineTo((float) x, midY - minPoints[(size_t) x] * halfH);
    waveform.closeSubPath();

    g.setColour(LookAndFeelSaturnish::accent.withAlpha(0.55f));
    g.fillPath(waveform);
    g.setColour(LookAndFeelSaturnish::accent.withAlpha(0.85f));
    g.strokePath(waveform, juce::PathStrokeType(1.0f));

    if (displayedSampleRate > 0.0 && displayedBuffer.getNumSamples() > 0)
    {
        const double seconds = (double) displayedBuffer.getNumSamples() / displayedSampleRate;
        g.setColour(LookAndFeelSaturnish::text.withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText(juce::String(seconds, 2) + "s", bounds.reduced(6.0f, 3.0f), juce::Justification::topRight);
    }
}
