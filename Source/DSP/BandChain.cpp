#include "BandChain.h"
#include <cmath>

BandChain::BandChain(juce::dsp::ConvolutionMessageQueue& sharedQueue, IRReshapeWorker& sharedReshapeWorker)
    : convolution(sharedQueue), reshapeWorker(sharedReshapeWorker)
{
    feedbackShaper.functionToUse = [] (float x) { return std::tanh(x); };
}

void BandChain::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    convolution.prepare(spec);
    preDelayLine.prepare(spec);

    dryWet.prepare(spec);
    dryWet.setMixingRule(juce::dsp::DryWetMixingRule::linear);
    dryWet.setWetLatency((float) convolution.getLatency());

    toneLowShelf.prepare(spec);
    toneHighShelf.prepare(spec);
    feedbackShaper.prepare(spec);
    feedbackDCBlocker.prepare(spec);

    feedbackLimiter.prepare(spec);
    feedbackLimiter.setThreshold(-3.0f);
    feedbackLimiter.setRelease(50.0f);

    feedbackTap.setSize((int) spec.numChannels, (int) spec.maximumBlockSize);
    feedbackTap.clear();

    applyToneCoefficients();

    auto dcCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 20.0);
    feedbackDCBlocker.state = dcCoeffs;

    needsInitialLoad = true;
    pendingReloadCountdown = 0; // force a load on the very first block
}

void BandChain::reset()
{
    convolution.reset();
    preDelayLine.reset();
    dryWet.reset();
    toneLowShelf.reset();
    toneHighShelf.reset();
    feedbackShaper.reset();
    feedbackLimiter.reset();
    feedbackDCBlocker.reset();
    feedbackTap.clear();
}

void BandChain::applyToneCoefficients()
{
    // Simple tilt EQ: tone in [-1, 1] maps to a low-shelf cut / high-shelf boost pair (or the
    // reverse), pivoting around 1kHz, +/-6dB at the extremes. Runtime-only, no IR reload needed.
    const float gainDb = current.tone * 6.0f;
    const float lowGain = juce::Decibels::decibelsToGain(-gainDb);
    const float highGain = juce::Decibels::decibelsToGain(gainDb);

    *toneLowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, 300.0, 0.707f, lowGain);
    *toneHighShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 3000.0, 0.707f, highGain);
}

void BandChain::applyLoadedIR(juce::AudioBuffer<float>&& shaped, double sr)
{
    // Must be called from the audio thread (see the header comment) -- JUCE's own docs for
    // Convolution say load calls "must be synchronised with process() calls, which in practice
    // means making the load() call from the audio thread." The call itself is wait-free (JUCE
    // defers the actual engine-building work to its own internal background thread via the
    // shared ConvolutionMessageQueue), so this doesn't reintroduce the CPU-spike problem.
    convolution.loadImpulseResponse(std::move(shaped), sr,
                                     juce::dsp::Convolution::Stereo::yes,
                                     juce::dsp::Convolution::Trim::no,
                                     juce::dsp::Convolution::Normalise::yes);
}

void BandChain::setMacroValues(const MacroValues& values)
{
    const bool toneChanged = ! juce::approximatelyEqual(values.tone, current.tone);

    const bool irRelevantChanged =
        values.irIndex != lastIrIndexForReload ||
        ! juce::approximatelyEqual(values.fadeInMs, lastFadeInForReload) ||
        ! juce::approximatelyEqual(values.fadeOutPercent, lastFadeOutForReload) ||
        ! juce::approximatelyEqual(values.stretch, lastStretchForReload);

    current = values;

    if (toneChanged)
        applyToneCoefficients();

    if (irRelevantChanged || needsInitialLoad)
    {
        lastIrIndexForReload = values.irIndex;
        lastFadeInForReload = values.fadeInMs;
        lastFadeOutForReload = values.fadeOutPercent;
        lastStretchForReload = values.stretch;
        pendingReloadCountdown = needsInitialLoad ? 0 : reloadDebounceSamplesAt48k;
        needsInitialLoad = false;
    }
}

void BandChain::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();

    // If the background worker has finished shaping a new IR, hand it to the convolution engine
    // here -- on the audio thread, as required (see applyLoadedIR()'s comment). This check is
    // just a spin-locked pointer scan across a handful of slots, not the actual shaping work.
    {
        juce::AudioBuffer<float> readyIR;
        double readySr = sampleRate;
        if (reshapeWorker.tryTakeResult(*this, readyIR, readySr))
            applyLoadedIR(std::move(readyIR), readySr);
    }

    // Debounced reshape trigger: coalesces rapid knob movement into one request ~40ms after the
    // last change. The request itself is a handful of POD fields handed to the shared background
    // worker (IRReshapeWorker) -- the actual disk read + resample + fade math never runs here, on
    // the audio thread. (It used to; that's what caused real CPU spikes/dropouts when dragging
    // Stretch or picking a new IR, confirmed via a user recording of Ableton's CPU meter hitting
    // 140%+ at the exact moment of a Stretch drag.)
    if (pendingReloadCountdown >= 0)
    {
        pendingReloadCountdown -= numSamples;
        if (pendingReloadCountdown <= 0)
        {
            pendingReloadCountdown = -1;

            IRReshapeWorker::Job job;
            job.irIndex = current.irIndex;
            job.sampleRate = sampleRate;
            job.fadeInMs = current.fadeInMs;
            job.fadeOutPercent = current.fadeOutPercent;
            job.stretch = current.stretch;
            reshapeWorker.requestReshape(*this, job);
        }
    }

    if (current.bypass)
        return;

    juce::dsp::AudioBlock<float> block(buffer);

    dryWet.setWetMixProportion(current.dryWetPercent * 0.01f);
    dryWet.pushDrySamples(block);

    // Add last block's shaped feedback tap into this block's input before it goes through
    // pre-delay + convolution again.
    const int fbSamples = juce::jmin(numSamples, feedbackTap.getNumSamples());
    for (int ch = 0; ch < buffer.getNumChannels() && ch < feedbackTap.getNumChannels(); ++ch)
        buffer.addFrom(ch, 0, feedbackTap, ch, 0, fbSamples);

    preDelayLine.setDelay((float) (current.preDelayMs * 0.001 * sampleRate));
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    preDelayLine.process(ctx);

    convolution.process(ctx);

    toneLowShelf.process(ctx);
    toneHighShelf.process(ctx);

    // Self-heal against non-finite samples (NaN/Inf) before they can reach the output or, worse,
    // get folded into the feedback tap and recirculate forever. An IIR filter (crossover, tone,
    // feedback DC blocker) that ever ingests a non-finite sample stays corrupted on every future
    // block -- y[n] depends on y[n-1] -- and neither changing a parameter nor the GUI's Reset
    // button (which only resets parameter *values*, not filter *state*) would clear that on its
    // own. If this fires, silence this block and reset every stateful DSP object in the chain so
    // the *next* block starts from clean state instead of staying poisoned indefinitely.
    bool nonFinite = false;
    for (int ch = 0; ch < buffer.getNumChannels() && ! nonFinite; ++ch)
    {
        auto* data = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            if (! std::isfinite(data[i])) { nonFinite = true; break; }
        }
    }

    if (nonFinite)
    {
        buffer.clear();
        reset();
    }

    // Shape this block's post-tone wet signal into the feedback tap for *next* block, so next
    // block's addition above is a plain add with no further processing needed mid-flight.
    feedbackTap.setSize(buffer.getNumChannels(), numSamples, false, false, true);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        feedbackTap.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    const float fbGain = current.feedbackPercent * 0.01f;
    feedbackTap.applyGain(fbGain);

    juce::dsp::AudioBlock<float> fbBlock(feedbackTap);
    juce::dsp::ProcessContextReplacing<float> fbCtx(fbBlock);
    feedbackShaper.process(fbCtx);
    feedbackLimiter.process(fbCtx);
    feedbackDCBlocker.process(fbCtx);

    dryWet.mixWetSamples(block);

    buffer.applyGain(juce::Decibels::decibelsToGain(current.outputGainDb));
}
