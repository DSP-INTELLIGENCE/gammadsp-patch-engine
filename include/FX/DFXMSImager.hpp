#pragma once
#include "DFXCore.hpp"
#include <algorithm>

class DFXMSImager : public DFXCore
{
public:
    DFXMSImager()
    {
        setWet(1.0f);
        setDry(1.0f);
        setMix(1.0f);
    }

    // -1..1  (negative narrows, positive widens)
    void setWidth(float w)
    {
        mWidth = std::clamp(w, -1.f, 1.f);
    }

    // Mid gain
    void setMidGain(float g)
    {
        mMidGain = std::max(0.f, g);
    }

    // Side gain
    void setSideGain(float g)
    {
        mSideGain = std::max(0.f, g);
    }

    float process(float x) override
    {
        // Convert mono-in to fake stereo
        float L = x;
        float R = x;

        float mid  = 0.5f * (L + R);
        float side = 0.5f * (L - R);

        side *= (1.f + mWidth);
        mid  *= mMidGain;
        side *= mSideGain;

        float y = mid + side;

        return mixOut(x, y);
    }

private:
    float mWidth = 0.f;
    float mMidGain = 1.f;
    float mSideGain = 1.f;
};
