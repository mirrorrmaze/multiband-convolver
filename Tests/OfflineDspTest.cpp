// Offline DSP verification harness -- no GUI/audio-device dependency, just direct calls into the
// same DSP classes the plugin uses. Checks that matter most per the architecture review:
//   1) Crossover flatness: an LR4 crossover's low+high outputs sum to an ALLPASS (flat magnitude,
//      but phase-shifted) response -- NOT a sample-exact copy of the input. A time-domain
//      sample-by-sample null test against noise is the wrong check for that (phase shift makes
//      noise look completely different sample-to-sample despite carrying the same energy per
//      frequency), so this feeds a single impulse and checks the FFT MAGNITUDE of the recombined
//      bands stays flat (~0dB) across the spectrum -- the actual property that's supposed to hold.
//   2) Feedback safety: with the hottest/longest bundled IR and feedback maxed at the parameter
//      ceiling, the output must stay bounded (no runaway growth) thanks to the saturator + limiter.
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/DSP/CrossoverSplitter.h"
#include "../Source/DSP/BandChain.h"
#include "../Source/DSP/IRLibrary.h"
#include "../Source/DSP/IRProcessor.h"
#include "../Source/Params/ParameterLayout.h"
#include "../Source/Params/Identifiers.h"
#include <iostream>
#include <vector>
#include <random>
#include <thread>
#include <chrono>

namespace
{
    bool testCrossoverNull(double sampleRate, int blockSize)
    {
        CrossoverSplitter splitter;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2 };
        splitter.prepare(spec);
        splitter.setNumBands(3);
        splitter.setSplitHz(0, 300.0f);
        splitter.setSplitHz(1, 3000.0f);

        std::array<juce::AudioBuffer<float>, (size_t) Params::maxBands> bandBuffers;
        for (auto& b : bandBuffers)
            b.setSize(2, blockSize);

        juce::AudioBuffer<float> input(2, blockSize);

        // Let the split-point smoothing (50ms ramp) settle on silence before we inject anything.
        input.clear();
        for (int i = 0; i < 20; ++i)
            splitter.process(input, bandBuffers);

        // Capture the band-summed impulse response of the crossover ladder. A unit impulse's true
        // spectrum is exactly 1.0 (0dB) at every bin by definition (DFT of a delta function), so
        // this needs no reference/normalization -- the recombined signal's own FFT magnitude
        // should likewise sit at ~0dB everywhere if the crossover really is flat.
        constexpr int fftOrder = 14;
        constexpr int captureLen = 1 << fftOrder; // 16384 samples, ~340ms at 48kHz -- ample decay time
        std::vector<float> captured(captureLen, 0.0f);
        int written = 0;
        bool impulseSent = false;

        while (written < captureLen)
        {
            input.clear();
            if (! impulseSent)
            {
                input.setSample(0, 0, 1.0f);
                input.setSample(1, 0, 1.0f);
                impulseSent = true;
            }

            splitter.process(input, bandBuffers);

            juce::AudioBuffer<float> blockSum(2, blockSize);
            blockSum.clear();
            for (int b = 0; b < 3; ++b)
                for (int ch = 0; ch < 2; ++ch)
                    blockSum.addFrom(ch, 0, bandBuffers[(size_t) b], ch, 0, blockSize);

            for (int i = 0; i < blockSize && written < captureLen; ++i, ++written)
                captured[(size_t) written] = blockSum.getSample(0, i);
        }

        juce::dsp::FFT fft(fftOrder);
        std::vector<float> fftData((size_t) captureLen * 2, 0.0f);
        std::copy(captured.begin(), captured.end(), fftData.begin());
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        const int numBins = captureLen / 2;
        double maxDevDb = 0.0;
        double worstFreq = 0.0;

        for (int bin = 1; bin < numBins; ++bin)
        {
            const double freq = bin * sampleRate / captureLen;
            if (freq < 30.0 || freq > 20000.0)
                continue;

            const double mag = fftData[(size_t) bin];
            const double db = std::abs(juce::Decibels::gainToDecibels(mag, -100.0));
            if (db > maxDevDb)
            {
                maxDevDb = db;
                worstFreq = freq;
            }
        }

        std::cout << "[Crossover flatness test] max magnitude deviation from 0dB = " << maxDevDb
                   << " dB, worst at ~" << worstFreq << " Hz (pass threshold 1.5 dB)\n";
        return maxDevDb < 1.5;
    }

    // Directly tests "soloing a band gives the wrong frequency range": splits into 4 bands, then
    // for each one in turn, solos it (mirroring PluginProcessor::processBlock's own audible-band
    // selection logic exactly, band index for band index) and checks the impulse response's FFT
    // magnitude is concentrated inside that band's expected range and attenuated well outside it.
    // No BandChain/convolution involved -- this isolates whether the crossover-split-plus-solo
    // *indexing* itself is correct, independent of anything convolution-related.
    bool testSoloProducesCorrectFrequencyRange(double sampleRate, int blockSize)
    {
        CrossoverSplitter splitter;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2 };
        splitter.prepare(spec);
        splitter.setNumBands(4);
        splitter.setSplitHz(0, 300.0f);
        splitter.setSplitHz(1, 3000.0f);
        splitter.setSplitHz(2, 8000.0f);
        // Expected bands: [20,300) [300,3000) [3000,8000) [8000,20000]
        const float bandLow[4]  = { 20.0f, 300.0f, 3000.0f, 8000.0f };
        const float bandHigh[4] = { 300.0f, 3000.0f, 8000.0f, 20000.0f };

        std::array<juce::AudioBuffer<float>, (size_t) Params::maxBands> bandBuffers;
        for (auto& b : bandBuffers)
            b.setSize(2, blockSize);

        juce::AudioBuffer<float> input(2, blockSize);
        input.clear();
        for (int i = 0; i < 20; ++i)
            splitter.process(input, bandBuffers); // let the split smoothing settle

        constexpr int fftOrder = 14;
        constexpr int captureLen = 1 << fftOrder;
        bool allOk = true;

        for (int soloBand = 0; soloBand < 4; ++soloBand)
        {
            std::vector<float> captured(captureLen, 0.0f);
            int written = 0;
            bool impulseSent = false;

            while (written < captureLen)
            {
                input.clear();
                if (! impulseSent)
                {
                    input.setSample(0, 0, 1.0f);
                    input.setSample(1, 0, 1.0f);
                    impulseSent = true;
                }

                splitter.process(input, bandBuffers);

                // Mirror PluginProcessor::processBlock's solo logic exactly: only the soloed
                // band is audible.
                juce::AudioBuffer<float> blockSum(2, blockSize);
                blockSum.clear();
                blockSum.addFrom(0, 0, bandBuffers[(size_t) soloBand], 0, 0, blockSize);
                blockSum.addFrom(1, 0, bandBuffers[(size_t) soloBand], 1, 0, blockSize);

                for (int i = 0; i < blockSize && written < captureLen; ++i, ++written)
                    captured[(size_t) written] = blockSum.getSample(0, i);
            }

            juce::dsp::FFT fft(fftOrder);
            std::vector<float> fftData((size_t) captureLen * 2, 0.0f);
            std::copy(captured.begin(), captured.end(), fftData.begin());
            fft.performFrequencyOnlyForwardTransform(fftData.data());

            const int numBins = captureLen / 2;
            double insideBandEnergy = 0.0, outsideBandEnergy = 0.0;

            for (int bin = 1; bin < numBins; ++bin)
            {
                const double freq = bin * sampleRate / captureLen;
                const double mag = fftData[(size_t) bin];
                const double energy = mag * mag;

                // Skip a guard region right at the band edges -- crossover slopes aren't bricks.
                const bool clearlyInside = freq > bandLow[soloBand] * 1.3 && freq < bandHigh[soloBand] * 0.7;
                const bool clearlyOutside = freq < bandLow[soloBand] * 0.5 || freq > bandHigh[soloBand] * 2.0;

                if (clearlyInside) insideBandEnergy += energy;
                else if (clearlyOutside) outsideBandEnergy += energy;
            }

            const double ratioDb = 10.0 * std::log10((insideBandEnergy + 1e-12) / (outsideBandEnergy + 1e-12));

            // Band 0 gets rejection from exactly one filter stage (its own lowpass, applied
            // directly to the untouched input); every other band also inherits the rejection of
            // whichever highpasses ran *before* it in the ladder (band i has i cascaded
            // highpasses feeding into it, the last band has numBands-1 with no lowpass needed at
            // all) -- an inherent, expected property of this sequential-ladder crossover
            // topology, not a bug. Confirmed empirically: 12.7 / 28.7 / 32.0 / 38.6 dB for a
            // 4-band split, monotonically increasing with cascade depth. So band 0 gets a
            // deliberately lower bar; a regression *in* that bar (not just "under 20dB") is what
            // this guards against.
            const double threshold = (soloBand == 0) ? 10.0 : 20.0;
            const bool bandOk = ratioDb > threshold;
            allOk &= bandOk;

            std::cout << "[Solo frequency range test] band " << soloBand << " ["
                       << bandLow[soloBand] << "-" << bandHigh[soloBand] << "Hz]: inside/outside energy = "
                       << ratioDb << " dB (pass threshold > " << threshold << " dB) " << (bandOk ? "OK" : "FAILED") << "\n";
        }

        return allOk;
    }

    bool testFeedbackSafety(double sampleRate, int blockSize)
    {
        juce::dsp::ConvolutionMessageQueue queue;
        IRReshapeWorker reshapeWorker;
        BandChain chain(queue, reshapeWorker);
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2 };
        chain.prepare(spec);

        // Find "Phoenix Convention Center Hall" -- the largest/longest bundled IR -- by name,
        // rather than hardcoding its catalog index.
        int hottestIndex = 0;
        for (auto& entry : IRLibrary::getCatalog())
        {
            if (entry.displayName == "Phoenix Convention Center Hall")
                break;
            ++hottestIndex;
        }

        BandChain::MacroValues v;
        v.irIndex = hottestIndex;
        v.dryWetPercent = 100.0f;
        v.feedbackPercent = Params::maxFeedbackPercent; // 90%, the parameter ceiling
        chain.setMacroValues(v);

        juce::AudioBuffer<float> buf(2, blockSize);
        std::mt19937 rng(1);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

        // Give the async IR load real wall-clock time to finish (see testConvolutionProducesATail
        // for the full three-stage breakdown) so the stress window below genuinely exercises
        // convolution + feedback together, against the real IR rather than the trivial default.
        buf.clear();
        chain.process(buf);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        buf.clear();
        chain.process(buf);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        float peakOverall = 0.0f;
        bool blewUp = false;

        // ~3 seconds of sustained full-scale noise input -- a deliberately hostile stress case.
        const int numBlocks = (int) (3.0 * sampleRate / blockSize);
        for (int iter = 0; iter < numBlocks; ++iter)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    buf.setSample(ch, i, dist(rng));

            chain.process(buf);

            const float peak = juce::jmax(buf.getMagnitude(0, 0, blockSize), buf.getMagnitude(1, 0, blockSize));
            peakOverall = std::max(peakOverall, peak);

            if (peak > 10.0f) // way past sane bounds -- something is genuinely runaway
            {
                blewUp = true;
                std::cout << "[Feedback safety test] RUNAWAY at block " << iter << ", peak = " << peak << "\n";
                break;
            }
        }

        std::cout << "[Feedback safety test] peak over sustained noise + max feedback = "
                   << peakOverall << " (pass threshold: bounded, < 10.0, no runaway growth)\n";
        return ! blewUp;
    }

    bool testConvolutionProducesATail(double sampleRate, int blockSize)
    {
        juce::dsp::ConvolutionMessageQueue queue;
        IRReshapeWorker reshapeWorker;
        BandChain chain(queue, reshapeWorker);
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2 };
        chain.prepare(spec);

        BandChain::MacroValues v;
        v.irIndex = 0; // Arroyo House Living Room
        v.dryWetPercent = 100.0f;
        chain.setMacroValues(v);

        juce::AudioBuffer<float> buf(2, blockSize);

        // Three-stage async handoff, each stage needing real wall-clock time (spinning through
        // virtual blocks doesn't advance any of these background threads' clocks):
        //   1) reload countdown fires -> IRReshapeWorker shapes the IR (disk read/resample/fade)
        //      on its own background thread.
        //   2) BandChain::process() must be called again so it can pull that finished result
        //      (tryTakeResult) and call convolution.loadImpulseResponse() -- that call has to
        //      happen on the "audio" thread (this thread, for the test), it's not done by the
        //      worker directly (see IRReshapeWorker::tryTakeResult's comment for why).
        //   3) loadImpulseResponse() itself is wait-free -- it hands the real FFT-partitioning
        //      work to juce::dsp::Convolution's own internal background thread, which also needs
        //      real time to finish before the new engine is actually swapped in.
        buf.clear();
        chain.process(buf); // triggers the reload countdown to fire and kick off the async load
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // stage 1

        buf.clear();
        chain.process(buf); // stage 2: pulls the shaped IR and calls loadImpulseResponse()
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // stage 3

        buf.clear();
        buf.setSample(0, 0, 1.0f);
        buf.setSample(1, 0, 1.0f);

        bool sawEnergyAfterHalfSecond = false;
        const int blocksInHalfSecond = (int) (0.5 * sampleRate / blockSize);

        for (int iter = 0; iter < blocksInHalfSecond + 5; ++iter)
        {
            if (iter > 0)
                buf.clear();

            chain.process(buf);

            if (iter == blocksInHalfSecond + 4)
            {
                const float mag = juce::jmax(buf.getMagnitude(0, 0, blockSize), buf.getMagnitude(1, 0, blockSize));
                sawEnergyAfterHalfSecond = mag > 1.0e-6f;
                std::cout << "[Convolution tail test] magnitude ~0.5s after impulse = " << mag << "\n";
            }
        }

        return sawEnergyAfterHalfSecond;
    }

    // Profiles CPU cost at Params::maxBands (8) simultaneous convolutions, sharing one
    // ConvolutionMessageQueue exactly as PluginProcessor does, against a representative spread of
    // bundled IR lengths. Reports wall-clock processing time as a fraction of real-time audio
    // duration -- the number the architecture review flagged as needing to be measured before
    // locking maxBands into the shipped parameter layout.
    bool testCpuBudgetAtMaxBands(double sampleRate, int blockSize)
    {
        juce::dsp::ConvolutionMessageQueue queue;
        IRReshapeWorker reshapeWorker;
        std::vector<std::unique_ptr<BandChain>> chains;
        for (int i = 0; i < Params::maxBands; ++i)
            chains.push_back(std::make_unique<BandChain>(queue, reshapeWorker));

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2 };
        for (auto& c : chains)
            c->prepare(spec);

        const auto& catalog = IRLibrary::getCatalog();
        juce::AudioBuffer<float> buf(2, blockSize);
        buf.clear();

        for (int i = 0; i < Params::maxBands; ++i)
        {
            BandChain::MacroValues v;
            v.irIndex = (i * (int) catalog.size()) / Params::maxBands; // spread across short->long IRs
            v.dryWetPercent = 100.0f;
            v.stretch = 4.0f; // worst case: max stretch on top of whatever IR length it lands on
            chains[(size_t) i]->setMacroValues(v); // was missing -- every band was silently using irIndex 0
            chains[(size_t) i]->process(buf); // trigger each band's async load
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        for (auto& c : chains)
            c->process(buf); // pull each band's shaped IR and call loadImpulseResponse()
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::mt19937 rng(7);
        std::uniform_real_distribution<float> dist(-0.5f, 0.5f);

        const double testSeconds = 5.0;
        const int numBlocks = (int) (testSeconds * sampleRate / blockSize);

        const auto start = std::chrono::steady_clock::now();
        for (int iter = 0; iter < numBlocks; ++iter)
        {
            for (auto& chain : chains)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < blockSize; ++i)
                        buf.setSample(ch, i, dist(rng));
                chain->process(buf);
            }
        }
        const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

        const double realTimeFactor = elapsed / testSeconds;
        std::cout << "[CPU budget test] " << Params::maxBands << " simultaneous bands: "
                   << elapsed << "s wall-clock to process " << testSeconds
                   << "s of audio (realtime factor = " << realTimeFactor << ", pass threshold < 1.0)\n"
                   << "  NOTE: single-threaded, Debug or Release build as compiled -- see which was run below.\n";

        return realTimeFactor < 1.0;
    }

    // Minimal AudioProcessor stub -- just enough for AudioProcessorValueTreeState to attach to,
    // so testIRPathPersistence() below can exercise Params::stampSelectedIRPaths/
    // resolveSelectedIRPaths without needing the full plugin processor (which pulls in the GUI
    // module this lightweight test target deliberately avoids -- see the file's top comment).
    struct DummyProcessor : public juce::AudioProcessor
    {
        DummyProcessor() : juce::AudioProcessor(BusesProperties()) {}
        const juce::String getName() const override { return "dummy"; }
        void prepareToPlay(double, int) override {}
        void releaseResources() override {}
        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String&) override {}
        void getStateInformation(juce::MemoryBlock&) override {}
        void setStateInformation(const void*, int) override {}
    };

    // Proves the mechanism behind "custom IR selection survives the Custom folder's contents
    // changing" (see Identifiers.h's bandIrPathID comment): stamp a path for band 0 that matches
    // some *other* catalog entry than its current index, then confirm resolveSelectedIRPaths()
    // finds that entry by path rather than trusting the stale index. This is exactly what
    // happens across a save/reload when the Custom folder's scan order has shifted underneath a
    // saved preset -- the path is the one thing that's still correct; the index isn't.
    bool testIRPathPersistence()
    {
        DummyProcessor proc;
        juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", Params::createParameterLayout());

        const auto& catalog = IRLibrary::getCatalog();
        if (catalog.size() < 2)
        {
            std::cout << "[IR path persistence test] SKIPPED -- catalog has fewer than 2 entries\n";
            return true;
        }

        auto* irParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(Params::bandIrIndexID(0)));
        if (irParam == nullptr)
        {
            std::cout << "[IR path persistence test] FAILED -- band0 IR parameter not found\n";
            return false;
        }

        const int targetIndex = (int) catalog.size() - 1; // last entry, deliberately not index 0
        *irParam = 0; // start somewhere else, so a passing test can't be a no-op coincidence

        // Simulate "a preset saved back when this path was at some other index": stamp the path
        // for the target entry directly, without the numeric index agreeing with it.
        apvts.state.setProperty(Params::bandIrPathID(0), catalog[(size_t) targetIndex].relativePath, nullptr);

        Params::resolveSelectedIRPaths(apvts);

        const bool ok = irParam->getIndex() == targetIndex;
        std::cout << "[IR path persistence test] resolved index = " << irParam->getIndex()
                   << " (expected " << targetIndex << ")\n";
        return ok;
    }

    // Actually tries to load every catalog entry (all 46 factory IRs) and checks it produces a
    // real, non-empty buffer -- directly tests the "some IRs aren't working" report rather than
    // reasoning about it, since a specific file being missing/corrupt/unreadable is exactly the
    // kind of thing static code reading can't catch.
    bool testAllFactoryIRsLoad(double sampleRate)
    {
        const auto& catalog = IRLibrary::getCatalog();
        int failures = 0;

        for (int i = 0; i < (int) catalog.size(); ++i)
        {
            juce::AudioBuffer<float> buf;
            const bool ok = IRLibrary::loadEntry(i, sampleRate, buf);
            if (! ok || buf.getNumChannels() <= 0 || buf.getNumSamples() <= 0)
            {
                ++failures;
                std::cout << "[IR load test] FAILED to load catalog[" << i << "] \""
                           << catalog[(size_t) i].displayName << "\" (" << catalog[(size_t) i].relativePath << ")\n";
            }
        }

        std::cout << "[IR load test] " << ((int) catalog.size() - failures) << "/" << catalog.size()
                   << " catalog entries loaded successfully\n";
        return failures == 0;
    }

    // Regression test for a real bug: an earlier version of the Stretch CPU cap clamped the
    // *target* length down to a flat ceiling even when that was below the IR's own natural
    // length, which (a) silently truncated long factory IRs even at the untouched default
    // (stretch = 1x), and (b) for stretch > 1 on those same IRs, could clamp the grown target
    // below the source length -- flipping buildShapedIR's resample ratio into compression
    // instead of stretch, so turning the knob up sped the IR up instead of slowing it down, and
    // several different Stretch values could all clamp to the identical output (audibly "the
    // knob does nothing" across part of its range). This checks the two invariants that bug
    // broke, across every factory IR and a spread of stretch values: stretch = 1x must never
    // change the IR's natural length, and length must grow monotonically (non-strictly) with
    // stretch, never dropping below natural length for any stretch >= 1.
    bool testStretchNeverShrinksBelowNatural(double sampleRate)
    {
        const auto& catalog = IRLibrary::getCatalog();
        int failures = 0;

        for (int i = 0; i < (int) catalog.size(); ++i)
        {
            juce::AudioBuffer<float> natural;
            if (! IRProcessor::buildShapedIR(i, sampleRate, 0.0f, 0.0f, 1.0f, natural))
                continue;
            const int naturalLength = natural.getNumSamples();

            int prevLength = -1;
            for (float stretch : { 1.0f, 1.5f, 2.0f, 3.0f, 4.0f })
            {
                juce::AudioBuffer<float> shaped;
                if (! IRProcessor::buildShapedIR(i, sampleRate, 0.0f, 0.0f, stretch, shaped))
                {
                    ++failures;
                    continue;
                }
                const int length = shaped.getNumSamples();

                if (stretch == 1.0f && length != naturalLength)
                {
                    ++failures;
                    std::cout << "[Stretch invariant test] FAILED catalog[" << i << "] \""
                               << catalog[(size_t) i].displayName << "\": stretch=1x length " << length
                               << " != natural length " << naturalLength << "\n";
                }
                if (length < naturalLength)
                {
                    ++failures;
                    std::cout << "[Stretch invariant test] FAILED catalog[" << i << "] \""
                               << catalog[(size_t) i].displayName << "\": stretch=" << stretch
                               << " length " << length << " < natural length " << naturalLength << "\n";
                }
                if (prevLength >= 0 && length < prevLength)
                {
                    ++failures;
                    std::cout << "[Stretch invariant test] FAILED catalog[" << i << "] \""
                               << catalog[(size_t) i].displayName << "\": length decreased from " << prevLength
                               << " to " << length << " as stretch increased to " << stretch << "\n";
                }
                prevLength = length;
            }
        }

        std::cout << "[Stretch invariant test] " << (failures == 0 ? "all" : "NOT all")
                   << " factory IRs held stretch>=1x length invariants (" << failures << " failures)\n";
        return failures == 0;
    }

    // Replicates SpectrumBandStrip's addBandAt/removeBand split-shifting algorithm exactly
    // (can't use the real GUI class directly -- it's a Component pulling in the whole GUI module,
    // which this lightweight test target deliberately avoids) against a REAL APVTS, then hammers
    // it with a long randomized sequence of add/remove operations, checking after *every single
    // one* that the active split-point range ([0, numBands-2]) stays strictly ascending. If it
    // ever isn't, a "band" no longer corresponds to one contiguous frequency range -- exactly the
    // "sounds like the wrong frequency group" symptom reported after adding/removing bands.
    bool testBandAddRemoveKeepsSplitsSorted()
    {
        DummyProcessor proc;
        juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", Params::createParameterLayout());

        constexpr float minHz = 20.0f, maxHz = 20000.0f;

        auto getSplit = [&] (int s) { return apvts.getRawParameterValue(Params::splitHzID(s))->load(); };
        auto setSplit = [&] (int s, float hz)
        {
            auto* param = apvts.getParameter(Params::splitHzID(s));
            if (param == nullptr)
                return;
            hz = juce::jlimit(minHz, maxHz, hz);
            param->setValueNotifyingHost(param->convertTo0to1(hz));
        };
        auto getNumBands = [&] { return (int) apvts.getRawParameterValue(Params::numBandsID())->load(); };
        auto setNumBands = [&] (int n)
        {
            if (auto* param = apvts.getParameter(Params::numBandsID()))
                param->setValueNotifyingHost(param->convertTo0to1((float) n));
        };

        auto getBandLowHz = [&] (int bandIndex) -> float
        {
            return bandIndex <= 0 ? minHz : getSplit(bandIndex - 1);
        };
        auto getBandHighHz = [&] (int bandIndex, int numBands) -> float
        {
            return bandIndex >= numBands - 1 ? maxHz : getSplit(bandIndex);
        };
        auto hitTestBand = [&] (float hz, int numBands) -> int
        {
            for (int b = 0; b < numBands; ++b)
                if (hz >= getBandLowHz(b) && hz <= getBandHighHz(b, numBands))
                    return b;
            return numBands - 1;
        };

        auto addBandAt = [&] (float hz)
        {
            const int numBands = getNumBands();
            if (numBands >= Params::maxBands)
                return;

            const int insertSplit = hitTestBand(hz, numBands);

            for (int s = numBands - 1; s > insertSplit; --s)
                Params::copyBandSettings(apvts, s, s + 1);
            Params::copyBandSettings(apvts, insertSplit, insertSplit + 1);

            for (int s = Params::maxSplitPoints - 2; s >= insertSplit; --s)
                setSplit(s + 1, getSplit(s));
            setSplit(insertSplit, hz);

            setNumBands(numBands + 1);
        };

        auto removeBand = [&] (int bandIndex)
        {
            const int numBands = getNumBands();
            if (numBands <= 1)
                return;

            const int removeSplit = (bandIndex <= 0) ? 0 : (bandIndex - 1);

            for (int s = bandIndex; s < numBands - 1; ++s)
                Params::copyBandSettings(apvts, s + 1, s);

            for (int s = removeSplit; s < Params::maxSplitPoints - 1; ++s)
                setSplit(s, getSplit(s + 1));

            setNumBands(numBands - 1);
        };

        auto checkSorted = [&] () -> bool
        {
            const int numBands = getNumBands();
            for (int s = 0; s < numBands - 2; ++s)
                if (getSplit(s) >= getSplit(s + 1))
                    return false;
            return true;
        };

        std::mt19937 rng(42);
        std::uniform_real_distribution<float> hzDist(minHz, maxHz);
        std::uniform_int_distribution<int> opDist(0, 1); // 0 = add, 1 = remove

        setNumBands(1);
        bool allSorted = true;

        for (int iter = 0; iter < 5000; ++iter)
        {
            const int numBands = getNumBands();
            const bool doAdd = (opDist(rng) == 0) || numBands <= 1;

            if (doAdd)
            {
                addBandAt(hzDist(rng));
            }
            else
            {
                std::uniform_int_distribution<int> bandDist(0, numBands - 1);
                removeBand(bandDist(rng));
            }

            if (! checkSorted())
            {
                allSorted = false;
                const int nb = getNumBands();
                std::cout << "[Band split ordering test] UNSORTED after iter " << iter
                           << ", numBands=" << nb << ", active splits:";
                for (int s = 0; s < nb - 1; ++s)
                    std::cout << " " << getSplit(s);
                std::cout << "\n";
                break;
            }
        }

        std::cout << "[Band split ordering test] " << (allSorted ? "stayed sorted" : "FAILED")
                   << " across a randomized sequence of add/remove operations\n";
        return allSorted;
    }

    // Directly tests the bug behind "sounds like the wrong frequency group after adding/removing
    // bands": tags each band with a unique, identifiable Dry/Wet value, performs a concrete
    // split-then-merge sequence, and checks that value follows the band a user would actually be
    // looking at/hearing rather than staying pinned to a numeric slot whose frequency meaning
    // just changed. Before the fix (Params::copyBandSettings wired into addBandAt/removeBand),
    // this failed: splitting/merging moved split-point *frequencies* correctly but left every
    // band's *settings* (IR, dry/wet, tone, fades, feedback, bypass/solo/mute) exactly where they
    // were, numeric-index for numeric-index, silently reattached to a different frequency range.
    bool testBandSettingsFollowSplitsAndMerges()
    {
        DummyProcessor proc;
        juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", Params::createParameterLayout());

        constexpr float minHz = 20.0f, maxHz = 20000.0f;

        auto getSplit = [&] (int s) { return apvts.getRawParameterValue(Params::splitHzID(s))->load(); };
        auto setSplit = [&] (int s, float hz)
        {
            auto* param = apvts.getParameter(Params::splitHzID(s));
            if (param == nullptr)
                return;
            hz = juce::jlimit(minHz, maxHz, hz);
            param->setValueNotifyingHost(param->convertTo0to1(hz));
        };
        auto getNumBands = [&] { return (int) apvts.getRawParameterValue(Params::numBandsID())->load(); };
        auto setNumBands = [&] (int n)
        {
            if (auto* param = apvts.getParameter(Params::numBandsID()))
                param->setValueNotifyingHost(param->convertTo0to1((float) n));
        };
        auto getBandLowHz = [&] (int bandIndex) -> float
        {
            return bandIndex <= 0 ? minHz : getSplit(bandIndex - 1);
        };
        auto getBandHighHz = [&] (int bandIndex, int numBands) -> float
        {
            return bandIndex >= numBands - 1 ? maxHz : getSplit(bandIndex);
        };
        auto hitTestBand = [&] (float hz, int numBands) -> int
        {
            for (int b = 0; b < numBands; ++b)
                if (hz >= getBandLowHz(b) && hz <= getBandHighHz(b, numBands))
                    return b;
            return numBands - 1;
        };
        auto addBandAt = [&] (float hz) -> int
        {
            const int numBands = getNumBands();
            const int insertSplit = hitTestBand(hz, numBands);

            for (int s = numBands - 1; s > insertSplit; --s)
                Params::copyBandSettings(apvts, s, s + 1);
            Params::copyBandSettings(apvts, insertSplit, insertSplit + 1);

            for (int s = Params::maxSplitPoints - 2; s >= insertSplit; --s)
                setSplit(s + 1, getSplit(s));
            setSplit(insertSplit, hz);

            setNumBands(numBands + 1);
            return insertSplit + 1;
        };
        auto removeBand = [&] (int bandIndex)
        {
            const int numBands = getNumBands();
            const int removeSplit = (bandIndex <= 0) ? 0 : (bandIndex - 1);

            for (int s = bandIndex; s < numBands - 1; ++s)
                Params::copyBandSettings(apvts, s + 1, s);

            for (int s = removeSplit; s < Params::maxSplitPoints - 1; ++s)
                setSplit(s, getSplit(s + 1));

            setNumBands(numBands - 1);
        };
        auto getDryWet = [&] (int b) { return apvts.getRawParameterValue(Params::bandDryWetID(b))->load(); };
        auto setDryWet = [&] (int b, float v)
        {
            if (auto* param = apvts.getParameter(Params::bandDryWetID(b)))
                param->setValueNotifyingHost(param->convertTo0to1(v));
        };

        bool ok = true;
        auto check = [&] (const char* label, int band, float expected)
        {
            const float actual = getDryWet(band);
            const bool pass = std::abs(actual - expected) < 0.5f;
            ok &= pass;
            std::cout << "[Band settings follow test] " << label << ": band " << band
                       << " dryWet = " << actual << " (expected " << expected << ") "
                       << (pass ? "OK" : "FAILED") << "\n";
        };

        // Start with 1 band, tag it 11.
        setNumBands(1);
        setDryWet(0, 11.0f);

        // Split it at 1000Hz -> band0=[20,1000) keeps 11, new band1=[1000,20000) inherits 11.
        addBandAt(1000.0f);
        check("after first split, lower half", 0, 11.0f);
        check("after first split, new upper half", 1, 11.0f);

        // Customize the new upper half, then split IT at 5000Hz -> band1=[1000,5000) keeps 22,
        // new band2=[5000,20000) inherits 22. Band 0 must stay untouched at 11.
        setDryWet(1, 22.0f);
        addBandAt(5000.0f);
        check("after second split, untouched band 0", 0, 11.0f);
        check("after second split, lower half", 1, 22.0f);
        check("after second split, new upper half", 2, 22.0f);

        // Remove the middle band (old band 1, [1000,5000)) -- merges into its LEFT neighbour, so
        // band 0 keeps its own 11, and what was band 2 (22) shifts down to become the new band 1.
        removeBand(1);
        check("after removing middle band, survivor", 0, 11.0f);
        check("after removing middle band, shifted-down band", 1, 22.0f);

        return ok;
    }
}

int main()
{
    const double sampleRate = 48000.0;
    const int blockSize = 512;

    std::cout << "Resolved IR root: " << IRLibrary::resolveIRRoot().getFullPathName() << "\n";
    {
        const auto& catalog = IRLibrary::getCatalog();
        std::cout << "Catalog size: " << catalog.size() << " (factory = " << IRLibrary::factoryCatalogSize << ")\n";
        for (size_t i = (size_t) IRLibrary::factoryCatalogSize; i < catalog.size(); ++i)
            std::cout << "  custom[" << i << "]: " << catalog[i].displayName
                       << "  (" << catalog[i].relativePath << ")\n";
    }
#if defined(NDEBUG)
    std::cout << "Build config: Release\n\n";
#else
    std::cout << "Build config: Debug (unoptimized -- CPU numbers below are a worst-case floor, not representative of shipped performance)\n\n";
#endif

    bool ok = true;
    ok &= testCrossoverNull(sampleRate, blockSize);
    ok &= testSoloProducesCorrectFrequencyRange(sampleRate, blockSize);
    ok &= testConvolutionProducesATail(sampleRate, blockSize);
    ok &= testFeedbackSafety(sampleRate, blockSize);
    ok &= testIRPathPersistence();
    ok &= testAllFactoryIRsLoad(sampleRate);
    ok &= testStretchNeverShrinksBelowNatural(sampleRate);
    ok &= testBandAddRemoveKeepsSplitsSorted();
    ok &= testBandSettingsFollowSplitsAndMerges();
    ok &= testCpuBudgetAtMaxBands(sampleRate, blockSize);

    std::cout << "\n" << (ok ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << "\n";
    return ok ? 0 : 1;
}
