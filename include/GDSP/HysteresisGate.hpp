#pragma once
#include <algorithm>


class HysteresisGate {
public:
    HysteresisGate()
    {
        reset();
    }

    void reset()
    {
        mState = false;
    }

    // Center threshold and width of hysteresis band
    void setThreshold(float t) { mThreshold = t; }
    void setWidth(float w)     { mWidth = std::max((float)0, w); }

    // Per-sample evaluation
    bool process(float x)
    {
        float hi = mThreshold + (float)0.5 * mWidth;
        float lo = mThreshold - (float)0.5 * mWidth;

        if (!mState && x >= hi)
            mState = true;
        else if (mState && x <= lo)
            mState = false;

        return mState;
    }

    bool state() const { return mState; }

private:
    float mThreshold = (float)0.5;
    float mWidth     = (float)0.1;
    bool   mState     = false;
};
