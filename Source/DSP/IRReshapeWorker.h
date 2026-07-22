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

private:
    struct Slot
    {
        BandChain* chain = nullptr;
        Job job;
        bool pending = false;
    };
    std::array<Slot, (size_t) Params::maxBands> slots;
    juce::SpinLock slotLock;

    void run() override;
};
