#include "IRProcessor.h"
#include "IRLibrary.h"

namespace IRProcessor
{
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
        const int stretchedLength = juce::jmax(1, (int) std::round(srcLength * stretch));
        outShapedIR.setSize(srcChannels, stretchedLength, false, false, true);

        const double ratio = (double) srcLength / (double) stretchedLength;
        for (int ch = 0; ch < srcChannels; ++ch)
        {
            juce::LagrangeInterpolator interpolator;
            interpolator.reset();
            interpolator.process(ratio, rawIR.getReadPointer(ch), outShapedIR.getWritePointer(ch), stretchedLength);
        }

        // Fade in: linear ramp from 0 over fadeInMs at the head of the IR.
        const int fadeInSamples = juce::jlimit(0, stretchedLength, (int) (fadeInMs * 0.001 * sampleRate));
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
        const float minKeepFraction = 0.02f; // never truncate to literally nothing
        const float keepFraction = 1.0f - (fadeOutPercent * 0.01f) * (1.0f - minKeepFraction);
        const int keptLength = juce::jlimit(64, stretchedLength, (int) std::round(stretchedLength * keepFraction));

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
