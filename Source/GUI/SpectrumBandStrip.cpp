#include "SpectrumBandStrip.h"
#include "LookAndFeelSaturnish.h"
#include "../Params/Identifiers.h"
#include "../Params/ParameterLayout.h"

namespace
{
    // Standard log-spaced reference frequencies for the background grid, Pro-Q-style.
    const float gridFrequencies[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f,
                                       1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };

    juce::String formatHz(float hz)
    {
        if (hz >= 1000.0f)
        {
            const float khz = hz / 1000.0f;
            const bool isWhole = std::abs(khz - std::round(khz)) < 0.005f;
            return (isWhole ? juce::String((int) std::round(khz)) : juce::String(khz, 1)) + " kHz";
        }
        return juce::String((int) std::round(hz)) + " Hz";
    }
}

SpectrumBandStrip::SpectrumBandStrip(MultibandConvolverAudioProcessor& processorToUse)
    : processor(processorToUse)
{
    setWantsKeyboardFocus(false);
    addChildComponent(edgePopup); // becomes visible on demand; added last so it paints on top
    startTimerHz(30);
}

void SpectrumBandStrip::showEdgePopup(int splitIndex, juce::Point<float> anchorPos)
{
    const float hz = processor.apvts.getRawParameterValue(Params::splitHzID(splitIndex))->load();

    juce::Rectangle<int> anchor((int) anchorPos.x - 1, (int) anchorPos.y - 1, 2, 2);
    edgePopup.showFor(anchor, formatHz(hz), hz, [this, splitIndex] (double v)
    {
        setSplitValue(splitIndex, (float) v);
    });
}

void SpectrumBandStrip::timerCallback()
{
    processor.inputAnalyzer.performFFTIfReady();
    repaint();
}

int SpectrumBandStrip::getNumBandsFromState() const
{
    return juce::jlimit(1, Params::maxBands, (int) processor.apvts.getRawParameterValue(Params::numBandsID())->load());
}

float SpectrumBandStrip::getBandLowHz(int bandIndex, int /*numBands*/) const
{
    if (bandIndex <= 0)
        return minHz;
    return processor.apvts.getRawParameterValue(Params::splitHzID(bandIndex - 1))->load();
}

float SpectrumBandStrip::getBandHighHz(int bandIndex, int numBands) const
{
    if (bandIndex >= numBands - 1)
        return maxHz;
    return processor.apvts.getRawParameterValue(Params::splitHzID(bandIndex))->load();
}

float SpectrumBandStrip::hzToX(float hz) const
{
    const float logMin = std::log10(minHz), logMax = std::log10(maxHz);
    const float t = (std::log10(juce::jlimit(minHz, maxHz, hz)) - logMin) / (logMax - logMin);
    return t * (float) getWidth();
}

float SpectrumBandStrip::xToHz(float x) const
{
    const float logMin = std::log10(minHz), logMax = std::log10(maxHz);
    const float t = juce::jlimit(0.0f, 1.0f, x / (float) juce::jmax(1, getWidth()));
    return std::pow(10.0f, logMin + t * (logMax - logMin));
}

int SpectrumBandStrip::hitTestEdge(float x) const
{
    const int numBands = getNumBandsFromState();
    for (int s = 0; s < numBands - 1; ++s)
    {
        const float splitHz = processor.apvts.getRawParameterValue(Params::splitHzID(s))->load();
        if (std::abs(hzToX(splitHz) - x) <= edgeGrabPx)
            return s;
    }
    return -1;
}

int SpectrumBandStrip::hitTestBand(float x) const
{
    const int numBands = getNumBandsFromState();
    const float hz = xToHz(x);
    for (int b = 0; b < numBands; ++b)
        if (hz >= getBandLowHz(b, numBands) && hz <= getBandHighHz(b, numBands))
            return b;
    return numBands - 1;
}

void SpectrumBandStrip::selectBand(int bandIndex)
{
    selectedBand = juce::jlimit(0, getNumBandsFromState() - 1, bandIndex);
    if (onBandSelected)
        onBandSelected(selectedBand);
    repaint();
}

void SpectrumBandStrip::setSplitValue(int splitIndex, float hz)
{
    auto* param = processor.apvts.getParameter(Params::splitHzID(splitIndex));
    if (param == nullptr)
        return;
    hz = juce::jlimit(minHz, maxHz, hz);
    param->setValueNotifyingHost(param->convertTo0to1(hz));
}

void SpectrumBandStrip::addBandAt(float hz)
{
    const int numBands = getNumBandsFromState();
    if (numBands >= Params::maxBands)
        return;

    const int bandIndex = hitTestBand(hzToX(hz));
    const int insertSplit = bandIndex;

    // Shift each existing band's *settings* (IR, dry/wet, tone, fades, feedback, bypass/solo/
    // mute -- everything band-specific) up by one slot, from the top down to just above the
    // split point, so they stay attached to the band a user is actually looking at/hearing
    // rather than to a numeric index whose frequency meaning is about to change. Both halves of
    // the split start as a copy of the band being split: the lower half (staying at
    // `insertSplit`) keeps its settings as-is below, the new upper half (`insertSplit + 1`) gets
    // an explicit copy right after the shift clears room for it.
    for (int s = numBands - 1; s > insertSplit; --s)
        Params::copyBandSettings(processor.apvts, s, s + 1);
    Params::copyBandSettings(processor.apvts, insertSplit, insertSplit + 1);

    // Shift existing split values up by one slot to make room, from the end down to insertSplit.
    for (int s = Params::maxSplitPoints - 2; s >= insertSplit; --s)
    {
        const float val = processor.apvts.getRawParameterValue(Params::splitHzID(s))->load();
        setSplitValue(s + 1, val);
    }
    setSplitValue(insertSplit, hz);

    auto* numBandsParamObj = processor.apvts.getParameter(Params::numBandsID());
    numBandsParamObj->setValueNotifyingHost(numBandsParamObj->convertTo0to1((float) (numBands + 1)));

    selectBand(insertSplit + 1);
}

void SpectrumBandStrip::removeBand(int bandIndex)
{
    const int numBands = getNumBandsFromState();
    if (numBands <= 1)
        return;

    const int removeSplit = (bandIndex <= 0) ? 0 : (bandIndex - 1);

    // Shift each surviving band's settings down by one slot to fill the gap left by the removed
    // band (see addBandAt's comment for the same reasoning in reverse). Band `bandIndex` itself
    // is discarded -- for band 0 that means the whole array shifts down and the old band 1
    // effectively becomes the new band 0 (matches "band 0 merges into its right neighbour"); for
    // any other band, its left neighbour (at `bandIndex - 1`) is left untouched and keeps its
    // own settings, matching "merges into its left neighbour".
    for (int s = bandIndex; s < numBands - 1; ++s)
        Params::copyBandSettings(processor.apvts, s + 1, s);

    for (int s = removeSplit; s < Params::maxSplitPoints - 1; ++s)
    {
        const float val = processor.apvts.getRawParameterValue(Params::splitHzID(s + 1))->load();
        setSplitValue(s, val);
    }

    auto* numBandsParamObj = processor.apvts.getParameter(Params::numBandsID());
    numBandsParamObj->setValueNotifyingHost(numBandsParamObj->convertTo0to1((float) (numBands - 1)));

    selectBand(juce::jmin(selectedBand, numBands - 2));
}

void SpectrumBandStrip::resized() {}

void SpectrumBandStrip::mouseMove(const juce::MouseEvent& e)
{
    const int edge = hitTestEdge((float) e.position.x);
    if (edge != hoveredEdge)
    {
        hoveredEdge = edge;
        setMouseCursor(edge >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SpectrumBandStrip::mouseDown(const juce::MouseEvent& e)
{
    const int edge = hitTestEdge((float) e.position.x);

    if (edge >= 0)
    {
        draggingSplitIndex = edge;
        if (auto* param = processor.apvts.getParameter(Params::splitHzID(edge)))
            param->beginChangeGesture();
        showEdgePopup(edge, e.position);
        return;
    }

    const int band = hitTestBand((float) e.position.x);

    if (e.mods.isRightButtonDown())
    {
        removeBand(band);
        return;
    }

    selectBand(band);
}

void SpectrumBandStrip::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingSplitIndex < 0)
        return;

    // Keep the dragged split from crossing its neighbours so bands can't invert/collapse.
    const int numBands = getNumBandsFromState();
    const float lowBound = (draggingSplitIndex == 0)
                                ? minHz
                                : processor.apvts.getRawParameterValue(Params::splitHzID(draggingSplitIndex - 1))->load();
    const float highBound = (draggingSplitIndex >= numBands - 2)
                                 ? maxHz
                                 : processor.apvts.getRawParameterValue(Params::splitHzID(draggingSplitIndex + 1))->load();

    float hz = xToHz((float) e.position.x);
    hz = juce::jlimit(lowBound * 1.02f, highBound * 0.98f, hz);
    setSplitValue(draggingSplitIndex, hz);
    showEdgePopup(draggingSplitIndex, e.position);
}

void SpectrumBandStrip::mouseUp(const juce::MouseEvent&)
{
    if (draggingSplitIndex >= 0)
    {
        if (auto* param = processor.apvts.getParameter(Params::splitHzID(draggingSplitIndex)))
            param->endChangeGesture();
        draggingSplitIndex = -1;
    }
}

void SpectrumBandStrip::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (hitTestEdge((float) e.position.x) >= 0)
        return;

    addBandAt(xToHz((float) e.position.x));
}

void SpectrumBandStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(LookAndFeelSaturnish::background);
    g.fillRect(bounds);

    // Frequency reference grid (Pro-Q-style): thin log-spaced vertical lines with Hz labels along
    // the bottom, drawn first so the spectrum/band tints read as sitting on top of it.
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    for (float hz : gridFrequencies)
    {
        const float x = hzToX(hz);
        g.setColour(LookAndFeelSaturnish::metalLight.withAlpha(0.16f));
        g.drawLine(x, 0.0f, x, bounds.getHeight(), 1.0f);

        g.setColour(LookAndFeelSaturnish::text.withAlpha(0.4f));
        g.drawText(formatHz(hz), (int) x + 3, (int) bounds.getHeight() - 14, 50, 12, juce::Justification::left);
    }

    const int numBands = getNumBandsFromState();

    // Live spectrum first, so the band tints sit as a translucent wash *over* it (Saturn-2 style)
    // rather than the spectrum being drawn on top of solid colour blocks.
    juce::Path spectrumFillPath;
    juce::Path spectrumLinePath;
    const auto& analyzer = processor.inputAnalyzer;
    const int numBins = SpectrumAnalyzer::numBins;
    const double sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0;

    bool started = false;
    float lastX = 0.0f;
    for (int i = 1; i < numBins; ++i)
    {
        const float freq = (float) (i * sampleRate / SpectrumAnalyzer::fftSize);
        if (freq < minHz || freq > maxHz)
            continue;

        const float x = hzToX(freq);
        const float db = analyzer.getMagnitudeDb(i);
        const float y = juce::jmap(juce::jlimit(-100.0f, 0.0f, db), -100.0f, 0.0f, bounds.getHeight(), 0.0f);

        if (! started)
        {
            spectrumLinePath.startNewSubPath(x, y);
            spectrumFillPath.startNewSubPath(x, bounds.getHeight());
            spectrumFillPath.lineTo(x, y);
            started = true;
        }
        else
        {
            spectrumLinePath.lineTo(x, y);
            spectrumFillPath.lineTo(x, y);
        }
        lastX = x;
    }

    if (started)
    {
        spectrumFillPath.lineTo(lastX, bounds.getHeight());
        spectrumFillPath.closeSubPath();

        juce::ColourGradient gradient(LookAndFeelSaturnish::accent.withAlpha(0.55f), 0.0f, 0.0f,
                                      LookAndFeelSaturnish::accent.withAlpha(0.02f), 0.0f, bounds.getHeight(),
                                      false);
        g.setGradientFill(gradient);
        g.fillPath(spectrumFillPath);

        g.setColour(LookAndFeelSaturnish::accent.withAlpha(0.85f));
        g.strokePath(spectrumLinePath, juce::PathStrokeType(1.5f));
    }

    // Band tint wash, low-opacity so the spectrum underneath stays clearly visible.
    for (int b = 0; b < numBands; ++b)
    {
        const float x0 = hzToX(getBandLowHz(b, numBands));
        const float x1 = hzToX(getBandHighHz(b, numBands));
        auto bandRect = juce::Rectangle<float>(x0, 0.0f, juce::jmax(1.0f, x1 - x0), bounds.getHeight());

        auto colour = LookAndFeelSaturnish::bandColour(b);
        const bool selected = (b == selectedBand);
        g.setColour(colour.withAlpha(selected ? 0.16f : 0.07f));
        g.fillRect(bandRect);

        if (selected)
        {
            g.setColour(colour.withAlpha(0.9f));
            g.drawRect(bandRect, 2.0f);
        }
    }

    // Draggable edges on top of everything.
    for (int s = 0; s < numBands - 1; ++s)
    {
        const float splitHz = processor.apvts.getRawParameterValue(Params::splitHzID(s))->load();
        const float x = hzToX(splitHz);
        const bool hot = (s == hoveredEdge || s == draggingSplitIndex);
        g.setColour(LookAndFeelSaturnish::text.withAlpha(hot ? 0.9f : 0.35f));
        g.drawLine(x, 0.0f, x, bounds.getHeight(), hot ? 2.5f : 1.5f);
    }
}
