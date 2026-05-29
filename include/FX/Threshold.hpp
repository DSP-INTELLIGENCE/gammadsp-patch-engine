#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class Threshold {
public:
    Threshold(Sample threshold = (Sample)0.5, Sample smoothTime = (Sample)0.02)
    {
        setThreshold(threshold);
        setSmoothTime(smoothTime);
    }

    void setThreshold(Sample t)
    {
        mThreshold = std::clamp(t, (Sample)0, (Sample)1);
    }

    void setSmoothTime(Sample seconds)
    {
        seconds = std::max((Sample)0.0001, seconds);
        mCoeff = std::exp((Sample)-1 / (seconds * (Sample)gam::sampleRate()));
    }

    void reset(Sample value = (Sample)0)
    {
        mValue = std::clamp(value, (Sample)0, (Sample)1);
    }

    // per-sample
    Sample process(Sample x)
    {
        Sample target = (x >= mThreshold) ? (Sample)1 : (Sample)0;
        mValue = mCoeff * mValue + ((Sample)1 - mCoeff) * target;
        return mValue;
    }

    bool isHigh() const { return mValue > (Sample)0.5; }
    Sample value() const { return mValue; }

private:
    Sample mThreshold = (Sample)0.5;
    Sample mCoeff     = (Sample)0;
    Sample mValue     = (Sample)0;
};
