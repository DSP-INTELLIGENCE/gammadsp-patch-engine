#pragma once
#include <cmath>
#include <algorithm>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

struct DriveCompander : public Function
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

    float process(float x) override
    {
        // smooth control
        amountLP += 0.002f * (amount - amountLP);

        // envelope follower
        float a = std::fabs(x);
        env += (a - env) * 0.01f;

        // loudness compensation curve
        float comp = 1.f / (1.f + env * 3.f * amountLP);

        return zap(x * comp);
    }
};
