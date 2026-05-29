#pragma once
#include "ZeroCrossRate.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

template<class Sample>
class PitchTrackerZCR {
public:
    PitchTracker()
    : mZCR((Sample)0.02)
    {
        reset();
    }

    void reset()
    {
        mPitch  = (Sample)0;
        mSmooth = (Sample)0;
        mZCR.reset();
    }

    // per-sample — returns pitch in Hz, 0 if undefined
    Sample process(Sample x)
    {
        // Feed ZCR
        Sample freq = mZCR.process(x);

        // Reject insane values
        if (freq < (Sample)40 || freq > (Sample)4000)
            freq = (Sample)0;

        // If undefined, gently fall toward zero
        if (freq == (Sample)0)
        {
            mSmooth *= (Sample)0.98;
        }
        else
        {
            // Smooth pitch changes
            mSmooth = (Sample)0.9 * mSmooth + (Sample)0.1 * freq;
        }

        // Kill tiny values
        if (mSmooth < (Sample)1)
            mSmooth = (Sample)0;

        mPitch = mSmooth;
        return mPitch;
    }

    Sample pitch() const { return mPitch; }

private:
    ZeroCrossRate<Sample> mZCR;

    Sample mPitch  = (Sample)0;
    Sample mSmooth = (Sample)0;
};
