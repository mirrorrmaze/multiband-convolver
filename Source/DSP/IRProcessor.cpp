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

        // Fade out: fadeOutPercent (0-100) of the tail is progressively tapered to silence, giving
        // a gated-reverb-style shortening at high settings.
        const int fadeOutSamples = juce::jlimit(0, stretchedLength, (int) (stretchedLength * (fadeOutPercent * 0.01f)));
        if (fadeOutSamples > 0)
        {
            const int fadeStart = stretchedLength - fadeOutSamples;
            for (int ch = 0; ch < srcChannels; ++ch)
            {
                auto* data = outShapedIR.getWritePointer(ch);
                for (int i = 0; i < fadeOutSamples; ++i)
                {
                    const float g = 1.0f - ((float) i / (float) fadeOutSamples);
                    data[fadeStart + i] *= g;
                }
            }
        }

        return true;
    }
}
