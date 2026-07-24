#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Params/Identifiers.h"
#include "IRReshapeWorker.h"

// Per-band processing chain: pre-delay -> convolution (against a selectable/reshapeable IR) ->
// tone (tilt EQ) -> feedback tap (saturator + limiter + DC blocker, fed back into the next
// block's input) -> dry/wet mix -> output trim.
//
// Fade in/out and stretch reshape the *stored IR buffer itself* (see architecture notes: this is
// the only way to actually get a gated/reverse-swell character or a real density change, a
// post-convolution effect can't fake it) and are re-applied via a debounced reload so dragging
// those knobs doesn't thrash the convolution engine's FFT partitioning every callback. Tone and
// dry/wet are cheap runtime operations and apply instantly.
class BandChain
{
public:
    struct MacroValues
    {
        int irIndex = 0;
        float dryWetPercent = 100.0f;
        float preDelayMs = 0.0f;
        float tone = 0.0f;            // -1 (dark) .. +1 (bright)
        float fadeInMs = 0.0f;
        float fadeOutPercent = 0.0f;  // 0 = untouched tail, 100 = heavily gated
        float stretch = 1.0f;         // IR length multiplier
        float feedbackPercent = 0.0f; // capped at Params::maxFeedbackPercent by the parameter range
        float outputGainDb = 0.0f;
        bool bypass = false;
    };

    BandChain(juce::dsp::ConvolutionMessageQueue& sharedQueue, IRReshapeWorker& sharedReshapeWorker);

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // Reads target macro values; cheap ones (tone/dryWet/preDelay/feedback/outputGain/bypass) take
    // effect immediately, IR-affecting ones (irIndex/fadeIn/fadeOut/stretch) are debounced and the
    // actual reshape work is handed off to the shared background worker -- never done inline here.
    void setMacroValues(const MacroValues& values);

    // Processes `buffer` (this band's already-crossover-split signal) in place.
    void process(juce::AudioBuffer<float>& buffer);

    // Called from process() (audio thread) once IRReshapeWorker has a reshaped IR ready. Must
    // stay on the audio thread -- see IRReshapeWorker::tryTakeResult()'s comment for why.
    void applyLoadedIR(juce::AudioBuffer<float>&& shaped, double sr);

private:
    void applyToneCoefficients();

    juce::dsp::Convolution convolution;
    juce::dsp::DelayLine<float> preDelayLine { 1 << 17 }; // up to ~2.7s of pre-delay headroom at 48kHz
    juce::dsp::DryWetMixer<float> dryWet;

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> toneLowShelf, toneHighShelf;
    juce::dsp::WaveShaper<float> feedbackShaper;
    juce::dsp::Limiter<float> feedbackLimiter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> feedbackDCBlocker;

    juce::AudioBuffer<float> feedbackTap;   // holds shaped feedback signal from the previous block

    IRReshapeWorker& reshapeWorker;

    MacroValues current;
    double sampleRate = 44100.0;

    int pendingReloadCountdown = -1; // samples remaining until a debounced reshape request fires; -1 = idle
    static constexpr int reloadDebounceSamplesAt48k = 48000 / 25; // ~40ms
    float lastFadeInForReload = -1.0f, lastFadeOutForReload = -1.0f, lastStretchForReload = -1.0f;
    int lastIrIndexForReload = -1;
    bool needsInitialLoad = true;
};
