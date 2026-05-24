#pragma once
#include <cmath>
#include <algorithm>

struct PhaseCorrector
{
    // strength 0..1
    float amount = 0.5f;
    float amountLP = 0.5f;

    // simple allpass-based correction
    float ap_z1 = 0.f;
    float ap_z2 = 0.f;

    inline float zap(float x)
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    // one-pole allpass core
    inline float allpass(float x, float a)
    {
        float y = -a * x + ap_z1;
        ap_z1 = x + a * y;
        return y;
    }

    float process(float x)
    {
        amountLP += 0.002f * (amount - amountLP);

        // dynamic coefficient
        float a = 0.2f + 0.6f * amountLP;

        // two cascaded allpass stages
        x = allpass(x, a);
        x = allpass(x, a * 0.7f);

        return zap(x);
    }

    void reset()
    {
        ap_z1 = ap_z2 = 0.f;
        amountLP = amount;
    }
};
