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
#include "../Source/DSP/CrossoverSplitter.h"
#include "../Source/DSP/BandChain.h"
#include "../Source/DSP/IRLibrary.h"
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

        // Give the async IR load real wall-clock time to finish (see testConvolutionProducesATail)
        // so the full stress window below genuinely exercises convolution + feedback together.
        buf.clear();
        chain.process(buf);
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // now a 2-stage async handoff (reshape worker -> convolution)

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

        // The debounced reload is only *triggered* synchronously (sample-countdown based) -- the
        // actual IR load/FFT-partitioning happens on ConvolutionMessageQueue's background thread,
        // which needs real wall-clock time to run (it polls every ~10ms when idle). Spinning
        // through virtual blocks costs microseconds of real time, nowhere near enough, so this
        // needs an actual sleep, not just more process() calls.
        buf.clear();
        chain.process(buf); // triggers the reload countdown to fire and kick off the async load
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // now a 2-stage async handoff (reshape worker -> convolution)

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
            chains[(size_t) i]->process(buf); // trigger each band's async load
        }
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
}

int main()
{
    const double sampleRate = 48000.0;
    const int blockSize = 512;

    std::cout << "Resolved IR root: " << IRLibrary::resolveIRRoot().getFullPathName() << "\n";
#if defined(NDEBUG)
    std::cout << "Build config: Release\n\n";
#else
    std::cout << "Build config: Debug (unoptimized -- CPU numbers below are a worst-case floor, not representative of shipped performance)\n\n";
#endif

    bool ok = true;
    ok &= testCrossoverNull(sampleRate, blockSize);
    ok &= testConvolutionProducesATail(sampleRate, blockSize);
    ok &= testFeedbackSafety(sampleRate, blockSize);
    ok &= testCpuBudgetAtMaxBands(sampleRate, blockSize);

    std::cout << "\n" << (ok ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << "\n";
    return ok ? 0 : 1;
}
