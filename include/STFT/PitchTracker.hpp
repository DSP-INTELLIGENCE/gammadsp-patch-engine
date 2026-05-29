#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

template <class Sample>
class PitchTracker {
public:
    explicit PitchTracker(unsigned windowMs = 40)
    {
        setWindowMs(windowMs);
        reset();
    }

    void setWindowMs(unsigned ms)
    {
        mWindowSamples = std::max(32u, (unsigned)(ms * (Sample)gam::sampleRate() * (Sample)0.001));
        mBuffer.assign(mWindowSamples, (Sample)0);
        mWrite = 0;
    }

    void reset()
    {
        std::fill(mBuffer.begin(), mBuffer.end(), (Sample)0);
        mPitch = (Sample)0;
        mConfidence = (Sample)0;
    }

    // Per-sample processing
    Sample process(Sample x)
    {
        mBuffer[mWrite++] = x;
        if (mWrite >= mWindowSamples) mWrite = 0;

        if (++mCounter < mWindowSamples)
            return mPitch;

        mCounter = 0;
        estimatePitch();
        return mPitch;
    }

    Sample pitch() const { return mPitch; }
    Sample confidence() const { return mConfidence; }

private:
    void estimatePitch()
    {
        const unsigned minLag = (unsigned)((Sample)gam::sampleRate() / (Sample)2000);
        const unsigned maxLag = (unsigned)((Sample)gam::sampleRate() / (Sample)40);

        Sample bestCorr = 0;
        unsigned bestLag = 0;

        for (unsigned lag = minLag; lag < maxLag; ++lag)
        {
            Sample corr = 0;
            for (unsigned i = 0; i < mWindowSamples - lag; ++i)
                corr += mBuffer[i] * mBuffer[i + lag];

            if (corr > bestCorr)
            {
                bestCorr = corr;
                bestLag = lag;
            }
        }

        if (bestLag > 0)
        {
            mPitch = (Sample)gam::sampleRate() / (Sample)bestLag;
            mConfidence = bestCorr / (Sample)mWindowSamples;
        }
        else
        {
            mPitch = (Sample)0;
            mConfidence = (Sample)0;
        }
    }

private:
    std::vector<Sample> mBuffer;
    unsigned mWindowSamples = 0;
    unsigned mWrite = 0;
    unsigned mCounter = 0;

    Sample mPitch = 0;
    Sample mConfidence = 0;
};

