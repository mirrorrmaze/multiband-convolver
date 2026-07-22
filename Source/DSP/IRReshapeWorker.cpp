#include "IRReshapeWorker.h"
#include "BandChain.h"

void IRReshapeWorker::run()
{
    while (! threadShouldExit())
    {
        bool didWork = false;

        for (auto& slot : slots)
        {
            BandChain* chain = nullptr;
            Job job;

            {
                const juce::SpinLock::ScopedLockType lock(slotLock);
                if (slot.pending)
                {
                    chain = slot.chain;
                    job = slot.job;
                    slot.pending = false;
                }
            }

            if (chain == nullptr)
                continue;

            didWork = true;

            juce::AudioBuffer<float> shaped;
            if (IRProcessor::buildShapedIR(job.irIndex, job.sampleRate, job.fadeInMs, job.fadeOutPercent, job.stretch, shaped))
                chain->applyLoadedIR(std::move(shaped), job.sampleRate);

            if (threadShouldExit())
                return;
        }

        if (! didWork)
            wait(30);
    }
}
