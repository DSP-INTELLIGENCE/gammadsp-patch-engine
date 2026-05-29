#pragma once
#include "DFXCore.hpp"
#include <cmath>
#include <algorithm>

class DFXAutoWidth : public DFXCore
{
public:
    DFXAutoWidth()
    {
        setWet(1.0f);
        setDry(1.0f);
        setMix(1.0f);
    }

    // 0..1 : how much automatic widening happens
    void setAmount(float a)
    {
        amount = std::clamp(a, 0.f, 1.f);
    }

    // base width applied even at low level
    void setBaseWidth(float w)
    {
        baseWidth = std::clamp(w, -1.f, 1.f);
    }

    // sensitivity: higher = reacts more strongly to level changes
    void setSensitivity(float s)
    {
        sensitivity = std::max(0.01f, s);
    }

    float process(float x) override
    {
        // mono → pseudo-stereo domain
        float L = x;
        float R = x;

        // envelope follower
        float a = std::fabs(x);
        env = 0.995f * env + 0.005f * a;

        // convert envelope to dynamic width
        float dyn = 1.f - std::exp(-env * sensitivity);
        float width = baseWidth + dyn * amount;

        width = std::clamp(width, -1.f, 1.f);

        // M/S
        float mid  = 0.5f * (L + R);
        float side = 0.5f * (L - R);

        side *= (1.f + width);

        float y = mid + side;

        return mixOut(x, y);
    }

    StereoSample widen(float x) override
    {
        // mono → pseudo-stereo domain
        float L = x;
        float R = x;

        // envelope follower
        float a = std::fabs(x);
        env = 0.995f * env + 0.005f * a;

        // convert envelope to dynamic width
        float dyn = 1.f - std::exp(-env * sensitivity);
        float width = baseWidth + dyn * amount;

        width = std::clamp(width, -1.f, 1.f);

        // M/S
        float mid  = 0.5f * (L + R);
        float side = 0.5f * (L - R);

        side *= (1.f + width);

        float y = mid + side;
        
        return StereoSample(mixOut(x, mid + side),mixOut(x,mid - side));
    }

private:
    float amount     = 0.6f;
    float baseWidth  = 0.0f;
    float sensitivity = 4.0f;

    float env = 0.f;
};
