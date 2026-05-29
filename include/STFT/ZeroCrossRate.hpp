#pragma once
#include <algorithm>

template <class Sample>
class ZeroCrossRate {
public:
    explicit ZeroCrossRate(Sample windowSeconds = (Sample)0.02)
    : mPrev((Sample)0),
      mValue((Sample)0)
    {
        setWindow(windowSeconds);
    }

    void setWindow(Sample seconds)
    {
        mWindowSamples = std::max(1u, (unsigned)(seconds * (Sample)gam::sampleRate()));
        mCounter   = 0;
        mCrossings = 0;
    }

    void reset()
    {
        mPrev = (Sample)0;
        mCounter   = 0;
        mCrossings = 0;
        mValue = (Sample)0;
    }

    // per-sample
    Sample process(Sample x)
    {
        bool crossed = (x >= (Sample)0 && mPrev < (Sample)0) ||
                       (x <  (Sample)0 && mPrev >= (Sample)0);

        mPrev = x;

        if (crossed) ++mCrossings;
        ++mCounter;

        if (mCounter >= mWindowSamples)
        {
            // Normalize: max crossings ≈ windowSamples / 2
            Sample norm = (Sample)mCrossings / ((Sample)0.5 * (Sample)mWindowSamples);
            mValue = std::clamp(norm, (Sample)0, (Sample)1);

            mCounter   = 0;
            mCrossings = 0;
        }

        return mValue;
    }

    // 0..1 roughness / noisiness measure
    Sample value() const { return mValue; }

private:
    unsigned mWindowSamples = 1;
    unsigned mCounter   = 0;
    unsigned mCrossings = 0;

    Sample mPrev  = (Sample)0;
    Sample mValue = (Sample)0;
};
