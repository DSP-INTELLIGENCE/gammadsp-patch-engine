#pragma once
#include <algorithm>
#include <cmath>

#include "EnvFollow.hpp"

class ActivityDetector: public Module {
public:
    ActivityDetector()
    : mEnv((float)80),
      mThreshold((float)0.02),
      mLevel((float)0),
      mActive(false),
      mHoldfloats(0),
      mHoldLeft(0)
    {
        setHoldTime((float)0.15); // 150 ms default
    }

    void setThreshold(float t)
    {
        mThreshold = std::max((float)0, t);
    }

    void setHoldTime(float seconds)
    {
        mHoldfloats = (unsigned)(seconds * (float)gam::sampleRate());
    }

    void reset()
    {
        mLevel = (float)0;
        mActive = false;
        mHoldLeft = 0;
    }

    // per-sample
    bool process(float x)
    {
        float e = mEnv.process(std::abs(x));

        // smooth activity level
        mLevel = (float)0.995 * mLevel + (float)0.005 * e;

        if (e > mThreshold)
        {
            mActive = true;
            mHoldLeft = mHoldfloats;
        }
        else if (mHoldLeft > 0)
        {
            --mHoldLeft;
        }
        else
        {
            mActive = false;
        }

        return mActive;
    }

    // continuous activity level (0..1)
    float level() const { return mLevel; }

    // hard state
    bool isActive() const { return mActive; }

private:
    EnvFollow<float> mEnv;

    float mThreshold;
    float mLevel;
    bool   mActive;

    unsigned mHoldfloats;
    unsigned mHoldLeft;
};
