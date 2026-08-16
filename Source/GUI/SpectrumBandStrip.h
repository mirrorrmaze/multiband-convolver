#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "KnobPopup.h"

// Saturn-2-style draggable band editor: a live spectrum analyzer background with contiguous,
// shared-edge band blocks layered on top. Owns the split-point array's UI interaction so the
// shared-edge invariant (dragging an edge moves both neighbours) can't be violated by any one
// band's logic. See CrossoverSplitter for the DSP-side split model this mirrors.
//
// Interactions:
//   - drag an internal edge  -> moves the shared split point between two bands
//   - click a band           -> selects it (notifies onBandSelected)
//   - double-click a band    -> inserts a new split at that point (adds a band), up to maxBands
//   - right-click a band     -> removes it (merges into its left neighbour, or right if it's band 0),
//                                as long as more than one band remains
class SpectrumBandStrip final : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumBandStrip(MultibandConvolverAudioProcessor& processorToUse);

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    std::function<void(int)> onBandSelected;

    int getSelectedBand() const noexcept { return selectedBand; }

    // Called after a full parameter reset (see PluginEditor's Reset button) so the strip doesn't
    // keep a now-out-of-range selected band index.
    void resetSelectionToDefault() { selectBand(0); }

private:
    void timerCallback() override;

    float hzToX(float hz) const;
    float xToHz(float x) const;

    int getNumBandsFromState() const;
    float getBandLowHz(int bandIndex, int numBands) const;
    float getBandHighHz(int bandIndex, int numBands) const;

    int hitTestEdge(float x) const;   // returns split index [0, numBands-2], or -1
    int hitTestBand(float x) const;   // returns band index, or -1

    void setSplitValue(int splitIndex, float hz);
    void addBandAt(float hz);
    void removeBand(int bandIndex);
    void selectBand(int bandIndex);

    // Shows/refreshes the Hz-readout bubble anchored at `anchorPos` for the given split, and
    // wires its type-to-set editing gesture back to setSplitValue.
    void showEdgePopup(int splitIndex, juce::Point<float> anchorPos);

    MultibandConvolverAudioProcessor& processor;

    int selectedBand = 0;
    int draggingSplitIndex = -1;
    int hoveredEdge = -1;

    KnobPopup edgePopup;

    static constexpr float edgeGrabPx = 7.0f;
    static constexpr float minHz = 20.0f;
    static constexpr float maxHz = 20000.0f;
};
