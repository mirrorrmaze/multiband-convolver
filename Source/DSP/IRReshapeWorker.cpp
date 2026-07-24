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

            // The shaping itself (disk read, resample, fade envelope) is the expensive part and
            // stays off the audio thread. The result just sits in the slot until BandChain::
            // process() picks it up and hands it to juce::dsp::Convolution itself -- that call
            // has to happen on the audio thread (see tryTakeResult()'s comment), so we don't call
            // into `chain` here at all.
            juce::AudioBuffer<float> shaped;
            if (IRProcessor::buildShapedIR(job.irIndex, job.sampleRate, job.fadeInMs, job.fadeOutPercent, job.stretch, shaped))
            {
                const juce::SpinLock::ScopedLockType lock(slotLock);
                slot.result = std::move(shaped);
                slot.resultSampleRate = job.sampleRate;
                slot.resultReady = true;
            }

            if (threadShouldExit())
                return;
        }

        if (! didWork)
            wait(30);
    }
}
