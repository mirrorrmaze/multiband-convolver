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

        // Natural shape: Stretch applied, but no fade -- this is what gives the waveform its
        // fixed horizontal scale (see header comment), so only Stretch/IR selection ever change
        // it, never Fade In/Out.
        juce::AudioBuffer<float> natural;
        if (IRProcessor::buildShapedIR(job.irIndex, job.sampleRate, 0.0f, 0.0f, job.stretch, natural))
        {
            int fadeInSamples = 0, keptLength = natural.getNumSamples();
            IRProcessor::computeFadeRegion(natural.getNumSamples(), job.sampleRate, job.fadeInMs, job.fadeOutPercent,
                                            fadeInSamples, keptLength);

            // Bake the fade-in ramp directly into the display copy -- same math buildShapedIR
            // uses -- so the taper is visible on the waveform itself rather than a separate marker.
            for (int ch = 0; ch < natural.getNumChannels(); ++ch)
            {
                auto* data = natural.getWritePointer(ch);
                for (int i = 0; i < fadeInSamples; ++i)
                    data[i] *= (float) i / (float) juce::jmax(1, fadeInSamples);
            }

            const juce::SpinLock::ScopedLockType lock(resultLock);
            pendingResultBuffer = std::move(natural);
            pendingResultSampleRate = job.sampleRate;
            pendingResultKeptLength = keptLength;
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
    int keptLength = displayedKeptLength;
    {
        const juce::SpinLock::ScopedLockType lock(resultLock);
        if (! resultPending)
            return;
        buf = std::move(pendingResultBuffer);
        sr = pendingResultSampleRate;
        keptLength = pendingResultKeptLength;
        resultPending = false;
    }

    displayedBuffer = std::move(buf);
    displayedSampleRate = sr;
    displayedKeptLength = keptLength;
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
    {
        cutoffColumn = width;
        return;
    }

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

    cutoffColumn = juce::jlimit(0, width, (int) ((juce::int64) width * displayedKeptLength / numSamples));
}

void IRWaveformView::resized()
{
    rebuildDisplayPoints();
}

namespace
{
    juce::Path buildWaveformPath(const std::vector<float>& minPoints, const std::vector<float>& maxPoints,
                                  int fromColumn, int toColumn, float midY, float halfH)
    {
        juce::Path path;
        if (toColumn <= fromColumn)
            return path;

        path.startNewSubPath((float) fromColumn, midY - maxPoints[(size_t) fromColumn] * halfH);
        for (int x = fromColumn + 1; x < toColumn; ++x)
            path.lineTo((float) x, midY - maxPoints[(size_t) x] * halfH);
        for (int x = toColumn - 1; x >= fromColumn; --x)
            path.lineTo((float) x, midY - minPoints[(size_t) x] * halfH);
        path.closeSubPath();
        return path;
    }
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
    const int width = (int) minPoints.size();
    const int cutoff = juce::jlimit(0, width, cutoffColumn);

    g.setColour(LookAndFeelSaturnish::metalDark.withAlpha(0.6f));
    g.drawHorizontalLine((int) midY, bounds.getX(), bounds.getRight());

    // Kept region (will actually play): normal accent colour.
    auto keptPath = buildWaveformPath(minPoints, maxPoints, 0, cutoff, midY, halfH);
    g.setColour(LookAndFeelSaturnish::accent.withAlpha(0.55f));
    g.fillPath(keptPath);
    g.setColour(LookAndFeelSaturnish::accent.withAlpha(0.85f));
    g.strokePath(keptPath, juce::PathStrokeType(1.0f));

    // Cut region (fade-out has truncated this away): dimmed grey, reads as "won't play".
    if (cutoff < width)
    {
        auto cutPath = buildWaveformPath(minPoints, maxPoints, cutoff, width, midY, halfH);
        g.setColour(LookAndFeelSaturnish::metalLight.withAlpha(0.35f));
        g.fillPath(cutPath);
        g.setColour(LookAndFeelSaturnish::metalLight.withAlpha(0.5f));
        g.strokePath(cutPath, juce::PathStrokeType(1.0f));

        g.setColour(LookAndFeelSaturnish::text.withAlpha(0.4f));
        g.drawVerticalLine(cutoff, bounds.getY() + 2.0f, bounds.getBottom() - 2.0f);
    }

    if (displayedSampleRate > 0.0 && displayedKeptLength > 0)
    {
        const double seconds = (double) displayedKeptLength / displayedSampleRate;
        g.setColour(LookAndFeelSaturnish::text.withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText(juce::String(seconds, 2) + "s", bounds.reduced(6.0f, 3.0f), juce::Justification::topRight);
    }
}
