#pragma once
#include <algorithm>
#include <cmath>

template<class Sample>
class MaxAbs {
public:
    explicit MaxAbs(Sample windowSeconds = 0.01f)
    : mCounter(0), mCurrentMax(0), mValue(0), mSmoothed(0)
    {
        setWindow(windowSeconds);
    }

    void setWindow(Sample seconds)
    {
        mWindowSamples = std::max(1u, (unsigned)(seconds * gam::sampleRate()));
        reset();
    }

    void setSmoothing(Sample coeff) { mSmoothCoeff = std::clamp(coeff, (Sample)0, (Sample)0.9999); }

    void reset()
    {
        mCounter = 0;
        mCurrentMax = 0;
        mValue = 0;
        mSmoothed = 0;
    }

    Sample process(Sample x)
    {
        Sample a = std::abs(x);
        mCurrentMax = std::max(mCurrentMax, a);
        ++mCounter;

        if (mCounter >= mWindowSamples) {
            mValue = mCurrentMax;
            mCounter = 0;
            mCurrentMax = 0;
        }

        // Optional smoothing for modulation safety
        mSmoothed = mSmoothCoeff * mSmoothed + ((Sample)1 - mSmoothCoeff) * mValue;
        return mSmoothed;
    }

    Sample value() const { return mSmoothed; }

private:
    unsigned mWindowSamples = 1;
    unsigned mCounter = 0;

    Sample mCurrentMax = 0;
    Sample mValue = 0;

    Sample mSmoothed = 0;
    Sample mSmoothCoeff = (Sample)0.95;
};
