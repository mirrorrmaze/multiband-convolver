#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include "IRProcessor.h"
#include "../Params/Identifiers.h"

class BandChain; // see BandChain.h -- only needs applyLoadedIR(), declared there

// Shared background thread that does the actual IR reshaping (disk read + resample + fade
// envelope) for every band, so the audio thread never blocks on it. The audio thread only ever
// calls requestReshape(), which just stashes a small POD job in a per-band slot and returns --
// the real work (potentially several ms for a long IR) happens here instead.
//
// One fixed slot per band (matched by BandChain pointer identity): a new request for a band that
// already has a pending job simply overwrites it, so rapid changes (e.g. dragging Stretch) never
// build an unbounded backlog and the worker only ever processes the latest snapshot.
class IRReshapeWorker : private juce::Thread
{
public:
    IRReshapeWorker() : juce::Thread("IR Reshape")
    {
        startThread(juce::Thread::Priority::background);
    }

    ~IRReshapeWorker() override
    {
        shutdown();
    }

    // Safe to call more than once (e.g. explicitly from the owner's destructor before its
    // BandChains are torn down, then again implicitly here) -- stopThread() is idempotent.
    void shutdown()
    {
        stopThread(3000);
    }

    struct Job
    {
        int irIndex = 0;
        double sampleRate = 44100.0;
        float fadeInMs = 0.0f;
        float fadeOutPercent = 0.0f;
        float stretch = 1.0f;
    };

    // Audio thread. Cheap: acquires a spin lock just long enough to copy a few POD fields.
    void requestReshape(BandChain& targetChain, const Job& job)
    {
        {
            const juce::SpinLock::ScopedLockType lock(slotLock);
            Slot* freeSlot = nullptr;
            for (auto& slot : slots)
            {
                if (slot.chain == &targetChain) { slot.job = job; slot.pending = true; notify(); return; }
                if (freeSlot == nullptr && slot.chain == nullptr)
                    freeSlot = &slot;
            }
            if (freeSlot != nullptr)
            {
                freeSlot->chain = &targetChain;
                freeSlot->job = job;
                freeSlot->pending = true;
            }
        }
        notify();
    }

    // Audio thread only -- called from BandChain::process(). If the background thread has
    // finished shaping an IR for `targetChain`, moves it into outBuffer/outSampleRate and
    // returns true (consuming it). This exists because juce::dsp::Convolution's threading
    // contract requires loadImpulseResponse() itself to be called from the same thread that
    // calls process() -- its "wait-free" guarantee means that call won't block, not that it's
    // safe to invoke concurrently from a separate thread. So the expensive shaping (disk read,
    // resample, fade envelope) happens here on the background thread, but the actual handoff
    // into the Convolution object happens back on the audio thread, in BandChain::process().
    bool tryTakeResult(BandChain& targetChain, juce::AudioBuffer<float>& outBuffer, double& outSampleRate)
    {
        const juce::SpinLock::ScopedLockType lock(slotLock);
        for (auto& slot : slots)
        {
            if (slot.chain == &targetChain && slot.resultReady)
            {
                outBuffer = std::move(slot.result);
                outSampleRate = slot.resultSampleRate;
                slot.resultReady = false;
                return true;
            }
        }
        return false;
    }

private:
    struct Slot
    {
        BandChain* chain = nullptr;
        Job job;
        bool pending = false;

        juce::AudioBuffer<float> result;
        double resultSampleRate = 44100.0;
        bool resultReady = false;
    };
    std::array<Slot, (size_t) Params::maxBands> slots;
    juce::SpinLock slotLock;

    void run() override;
};
