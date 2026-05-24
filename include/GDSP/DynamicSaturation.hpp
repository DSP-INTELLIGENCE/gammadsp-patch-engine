#pragma once
#include <cmath>
#include <algorithm>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

struct DynamicSaturation : public Function
{
    // user control 0..1
    float amount = 0.5f;
    float amountLP = 0.5f;

    // envelope follower
    float env = 0.f;

    inline float zap(float x)
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    inline float sat(float x)
    {
        return x / (1.f + std::fabs(x));
    }

    float process(float x) override
    {
        // smooth control
        amountLP += 0.002f * (amount - amountLP);

        // slow envelope follower
        float a = std::fabs(x);
        env += (a - env) * 0.01f;

        // dynamic drive amount
        float drive = 1.f + env * 8.f * amountLP;

        // apply dynamic saturation
        float y = sat(x * drive);

        // preserve original body
        float blend = amountLP * 0.6f;
        y = x * (1.f - blend) + y * blend;

        return zap(y);
    }
};
