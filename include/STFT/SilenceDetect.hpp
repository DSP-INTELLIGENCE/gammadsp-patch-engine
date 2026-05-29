#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class SilenceDetect {
public:
    SilenceDetect(Sample threshold = (Sample)0.001, unsigned holdSamples = 2048)
    {
        setThreshold(threshold);
        setHold(holdSamples);
        reset();
    }

    void setThreshold(Sample t)
    {
        mThreshold = std::max(t, (Sample)0);
    }

    void setHold(unsigned samples)
    {
        mHold = std::max(1u, samples);
    }

    void reset()
    {
        mCount  = 0;
        mSilent = false;
    }

    // per-sample
    bool process(Sample x)
    {
        if (std::abs(x) < mThreshold)
        {
            if (++mCount >= mHold)
                mSilent = true;
        }
        else
        {
            mCount  = 0;
            mSilent = false;
        }

        return mSilent;
    }

    // true when silence is confirmed
    bool done() const { return mSilent; }

private:
    Sample   mThreshold = (Sample)0;
    unsigned mHold      = 1;
    unsigned mCount     = 0;
    bool     mSilent    = false;
};
