#include "IRProcessor.h"
#include "IRLibrary.h"

namespace IRProcessor
{
    void computeFadeRegion(int naturalLength, double sampleRate, float fadeInMs, float fadeOutPercent,
                            int& outFadeInSamples, int& outKeptLength)
    {
        outFadeInSamples = juce::jlimit(0, naturalLength, (int) (fadeInMs * 0.001 * sampleRate));

        const float minKeepFraction = 0.02f; // never truncate to literally nothing
        const float keepFraction = 1.0f - (fadeOutPercent * 0.01f) * (1.0f - minKeepFraction);
        outKeptLength = juce::jlimit(64, juce::jmax(64, naturalLength), (int) std::round(naturalLength * keepFraction));
    }

    bool buildShapedIR(int irIndex, double sampleRate, float fadeInMs, float fadeOutPercent, float stretch,
                        juce::AudioBuffer<float>& outShapedIR)
    {
        juce::AudioBuffer<float> rawIR;
        if (! IRLibrary::loadEntry(irIndex, sampleRate, rawIR))
            return false;

        const int srcChannels = rawIR.getNumChannels();
        const int srcLength = rawIR.getNumSamples();
        if (srcChannels <= 0 || srcLength <= 0)
            return false;

        // Stretch: resample the IR buffer length by `stretch` (0.25x - 4x). This shifts the
        // perceived density/pitch of the tail rather than doing a true time-stretch -- a
        // deliberate, pragmatic trade-off (see architecture notes) since reverb tails are
        // diffuse/unpitched content where that artifact is least perceptible.
        //
        // Growth is capped in absolute duration (not just the 0.25x-4x multiplier) because
        // juce::dsp::Convolution's per-block cost scales with IR length: the library's longer
        // factory IRs (up to ~11.6s) at 4x stretch would build a ~46s kernel, and that's real,
        // sustained CPU cost once loaded, not a one-off spike a bigger debounce window can fix.
        //
        // The cap floor is the source's own natural length, NOT a flat ceiling -- clamping the
        // target length below the natural source length was an earlier bug here: it silently
        // truncated any factory IR already longer than the cap even at the untouched default
        // (stretch = 1x), and for stretch > 1 on those same IRs it could clamp the *grown* target
        // below the *source* length, flipping the resample ratio into compression instead of
        // stretch -- so turning Stretch up would speed the IR up instead of slowing it down, and
        // since many different Stretch values all clamped to the same capped output, the knob
        // would audibly do nothing across a chunk of its range. Anchoring the ceiling at
        // max(natural length, cap) means Stretch can only ever grow something, never shrink it
        // below what picking that IR at 1x already gives you.
        constexpr double maxGrowthSeconds = 8.0;
        const int growthCeilingSamples = juce::jmax(srcLength, (int) (maxGrowthSeconds * sampleRate));
        const int stretchedLength = juce::jmin(growthCeilingSamples, juce::jmax(1, (int) std::round(srcLength * stretch)));
        outShapedIR.setSize(srcChannels, stretchedLength, false, false, true);

        const double ratio = (double) srcLength / (double) stretchedLength;
        for (int ch = 0; ch < srcChannels; ++ch)
        {
            juce::LagrangeInterpolator interpolator;
            interpolator.reset();
            interpolator.process(ratio, rawIR.getReadPointer(ch), outShapedIR.getWritePointer(ch), stretchedLength);
        }

        // Fade in/out region: shared with IRWaveformView (see computeFadeRegion's header comment)
        // so the waveform overlay can never drift out of sync with what actually gets shaped here.
        int fadeInSamples = 0, keptLength = stretchedLength;
        computeFadeRegion(stretchedLength, sampleRate, fadeInMs, fadeOutPercent, fadeInSamples, keptLength);

        // Fade in: linear ramp from 0 over fadeInMs at the head of the IR.
        for (int ch = 0; ch < srcChannels; ++ch)
        {
            auto* data = outShapedIR.getWritePointer(ch);
            for (int i = 0; i < fadeInSamples; ++i)
                data[i] *= (float) i / (float) juce::jmax(1, fadeInSamples);
        }

        // Fade out: a decay/length control, like Kilohearts Convolver's -- it actually shortens
        // the IR's audible tail rather than just tapering a fraction of the existing one in place
        // (which left the buffer's stored length unchanged even at 100%, so cranking it never
        // actually sounded short/gated -- just a gradual decline across the full original tail).
        // 0% keeps the full natural tail; 100% gates it down to a small fraction of its original
        // length. A short fade-to-zero right at the new cut point avoids a click from the
        // truncation itself.
        if (keptLength < stretchedLength)
        {
            const int clickFadeSamples = juce::jmin(keptLength, (int) (0.01 * sampleRate)); // ~10ms
            const int fadeStart = keptLength - clickFadeSamples;
            for (int ch = 0; ch < srcChannels; ++ch)
            {
                auto* data = outShapedIR.getWritePointer(ch);
                for (int i = 0; i < clickFadeSamples; ++i)
                {
                    const float g = 1.0f - ((float) i / (float) clickFadeSamples);
                    data[fadeStart + i] *= g;
                }
            }

            outShapedIR.setSize(srcChannels, keptLength, true, false, true); // true = keep existing content
        }

        return true;
    }
}
