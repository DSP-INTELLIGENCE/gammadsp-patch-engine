#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class ControlSmoother {
public:
    ControlSmoother()
    {
        reset();
    }

    void reset()
    {
        mValue = (Sample)0;
    }

    // 0..1 → slow..fast response
    void setSmoothness(Sample s)
    {
        mSmooth = std::clamp(s, (Sample)0.0001, (Sample)1);
    }

    // Set new target
    void setTarget(Sample t)
    {
        mTarget = t;
    }

    // Per-sample smoothing
    Sample process()
    {
        mValue += mSmooth * (mTarget - mValue);
        return mValue;
    }

    Sample value() const { return mValue; }

private:
    Sample mTarget = (Sample)0;
    Sample mValue  = (Sample)0;
    Sample mSmooth = (Sample)0.05;
};
