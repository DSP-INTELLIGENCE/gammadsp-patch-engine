#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class PitchStabilityMeter {
public:
    PitchStabilityMeter()
    {
        reset();
    }

    void reset()
    {
        mPrevPitch = (Sample)0;
        mVariance  = (Sample)0;
        mStability = (Sample)0;
    }

    // Feed the current pitch estimate (Hz, 0 if undefined)
    Sample process(Sample pitch)
    {
        if (pitch <= (Sample)0)
        {
            mVariance  *= (Sample)0.95;
            mStability *= (Sample)0.95;
            return mStability;
        }

        Sample delta = std::abs(pitch - mPrevPitch);

        // Normalized relative change
        Sample normDelta = delta / std::max(pitch, (Sample)1);

        // Integrate variance over time
        mVariance = (Sample)0.98 * mVariance + (Sample)0.02 * normDelta;

        // Convert variance into stability
        Sample rawStability = (Sample)1 - std::min(mVariance * (Sample)12, (Sample)1);

        // Smooth output
        mStability = (Sample)0.9 * mStability + (Sample)0.1 * rawStability;

        mPrevPitch = pitch;
        return mStability;
    }

    // 0..1 — how stable the pitch currently is
    Sample stability() const { return mStability; }

private:
    Sample mPrevPitch = (Sample)0;
    Sample mVariance  = (Sample)0;
    Sample mStability = (Sample)0;
};
