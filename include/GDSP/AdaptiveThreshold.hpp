#pragma once
#include <algorithm>


class AdaptiveThreshold {
public:
    AdaptiveThreshold()
    {
        reset();
    }

    void reset()
    {
        mMean = (float)0;
        mVar  = (float)0;
        mThreshold = (float)0;
    }

    // Controls how fast the model adapts (0..1)
    void setAdaptRate(float r)
    {
        mAdapt = std::clamp(r, (float)0.0001, (float)1);
    }

    // Controls how far above the mean the threshold is placed
    void setSensitivity(float s)
    {
        mSensitivity = std::max((float)0, s);
    }

    // Feed continuous signal
    float process(float x)
    {
        // Running mean & variance
        float delta = x - mMean;
        mMean += mAdapt * delta;
        mVar  += mAdapt * (std::abs(delta) - mVar);

        // Place threshold relative to statistics
        mThreshold = mMean + mSensitivity * mVar;

        return mThreshold;
    }

    float value() const { return mThreshold; }
    float mean() const { return mMean; }
    float variance() const { return mVar; }

private:
    float mMean       = (float)0;
    float mVar        = (float)0;
    float mThreshold  = (float)0;

    float mAdapt      = (float)0.01;
    float mSensitivity= (float)1.5;
};
