#include "ParameterLayout.h"
#include "Identifiers.h"
#include "../DSP/IRLibrary.h"

namespace Params
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        auto floatParam = [&params] (const juce::String& id, const juce::String& name,
                                      juce::NormalisableRange<float> range, float defaultValue,
                                      const juce::String& unit = {})
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { id, v1 }, name, range, defaultValue,
                juce::AudioParameterFloatAttributes().withLabel(unit)));
        };

        auto boolParam = [&params] (const juce::String& id, const juce::String& name, bool defaultValue)
        {
            params.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID { id, v1 }, name, defaultValue));
        };

        // ---- Global ----
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { numBandsID(), v1 }, "Number of Bands", 1, maxBands, 3));

        floatParam(inputGainID(), "Input Gain", { -24.0f, 24.0f, 0.01f }, 0.0f, "dB");
        floatParam(outputGainID(), "Output Gain", { -24.0f, 24.0f, 0.01f }, 0.0f, "dB");

        // Split points spread across the audible spectrum by default so the first N-1 splits
        // give a sensible starting layout for whatever numBands the user starts with.
        static const float defaultSplits[maxSplitPoints] = {
            300.0f, 3000.0f, 6000.0f, 9000.0f, 12000.0f, 15000.0f, 18000.0f
        };

        juce::NormalisableRange<float> splitRange { 20.0f, 20000.0f, 1.0f };
        splitRange.setSkewForCentre(1000.0f);

        for (int i = 0; i < maxSplitPoints; ++i)
            floatParam(splitHzID(i), "Split " + juce::String(i + 1), splitRange, defaultSplits[i], "Hz");

        // ---- Per band ----
        juce::StringArray irChoices;
        for (auto& entry : IRLibrary::getCatalog())
            irChoices.add(entry.displayName);

        juce::NormalisableRange<float> preDelayRange { 0.0f, 250.0f, 0.01f };
        juce::NormalisableRange<float> fadeInRange { 0.0f, 500.0f, 0.01f };
        juce::NormalisableRange<float> stretchRange { 0.25f, 4.0f, 0.001f };
        stretchRange.setSkewForCentre(1.0f);

        for (int b = 0; b < maxBands; ++b)
        {
            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID { bandIrIndexID(b), v1 },
                "Band " + juce::String(b + 1) + " IR",
                irChoices, 0));

            floatParam(bandDryWetID(b), "Band " + juce::String(b + 1) + " Dry/Wet",
                       { 0.0f, 100.0f, 0.1f }, 50.0f, "%");

            floatParam(bandPreDelayID(b), "Band " + juce::String(b + 1) + " Pre-Delay",
                       preDelayRange, 0.0f, "ms");

            floatParam(bandToneID(b), "Band " + juce::String(b + 1) + " Tone",
                       { -1.0f, 1.0f, 0.001f }, 0.0f);

            floatParam(bandFadeInID(b), "Band " + juce::String(b + 1) + " Fade In",
                       fadeInRange, 0.0f, "ms");

            floatParam(bandFadeOutID(b), "Band " + juce::String(b + 1) + " Fade Out",
                       { 0.0f, 100.0f, 0.1f }, 0.0f, "%");

            floatParam(bandStretchID(b), "Band " + juce::String(b + 1) + " Stretch",
                       stretchRange, 1.0f, "x");

            // Ceiling enforced here, not just downstream in the DSP -- host automation can never
            // push this parameter past the safe zone (see Identifiers.h maxFeedbackPercent).
            floatParam(bandFeedbackID(b), "Band " + juce::String(b + 1) + " Feedback",
                       { 0.0f, maxFeedbackPercent, 0.1f }, 0.0f, "%");

            floatParam(bandOutGainID(b), "Band " + juce::String(b + 1) + " Output Gain",
                       { -24.0f, 24.0f, 0.01f }, 0.0f, "dB");

            boolParam(bandBypassID(b), "Band " + juce::String(b + 1) + " Bypass", false);
            boolParam(bandSoloID(b), "Band " + juce::String(b + 1) + " Solo", false);
            boolParam(bandMuteID(b), "Band " + juce::String(b + 1) + " Mute", false);
        }

        return { params.begin(), params.end() };
    }

    void stampSelectedIRPaths(juce::AudioProcessorValueTreeState& apvts)
    {
        const auto& catalog = IRLibrary::getCatalog();

        for (int b = 0; b < maxBands; ++b)
        {
            auto* irParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(bandIrIndexID(b)));
            if (irParam == nullptr)
                continue;

            const int idx = irParam->getIndex();
            const juce::String path = (idx >= 0 && idx < (int) catalog.size())
                                         ? catalog[(size_t) idx].relativePath
                                         : juce::String();
            apvts.state.setProperty(bandIrPathID(b), path, nullptr);
        }
    }

    void resolveSelectedIRPaths(juce::AudioProcessorValueTreeState& apvts)
    {
        const auto& catalog = IRLibrary::getCatalog();

        for (int b = 0; b < maxBands; ++b)
        {
            const auto path = apvts.state.getProperty(bandIrPathID(b), {}).toString();
            if (path.isEmpty())
                continue;

            for (int i = 0; i < (int) catalog.size(); ++i)
            {
                if (catalog[(size_t) i].relativePath == path)
                {
                    if (auto* irParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(bandIrIndexID(b))))
                        *irParam = i;
                    break;
                }
            }
        }
    }

    void copyBandSettings(juce::AudioProcessorValueTreeState& apvts, int fromBand, int toBand)
    {
        if (fromBand == toBand)
            return;

        using IdFn = juce::String (*) (int);
        static const IdFn idFns[] = {
            bandIrIndexID, bandDryWetID, bandPreDelayID, bandToneID, bandFadeInID, bandFadeOutID,
            bandStretchID, bandFeedbackID, bandOutGainID, bandBypassID, bandSoloID, bandMuteID
        };

        for (auto* idFn : idFns)
        {
            auto* fromParam = apvts.getParameter(idFn(fromBand));
            auto* toParam = apvts.getParameter(idFn(toBand));
            if (fromParam == nullptr || toParam == nullptr)
                continue;

            // getValue()/setValueNotifyingHost() work in each parameter's own normalised [0,1]
            // representation, which is the one thing every parameter type (float, int, choice,
            // bool) shares -- lets this copy generically across all of them without needing a
            // switch on parameter type.
            toParam->setValueNotifyingHost(fromParam->getValue());
        }

        // The IR path property isn't a registered parameter (see bandIrPathID's comment), so it
        // needs its own copy directly on the state tree.
        apvts.state.setProperty(bandIrPathID(toBand), apvts.state.getProperty(bandIrPathID(fromBand), {}), nullptr);
    }
}
