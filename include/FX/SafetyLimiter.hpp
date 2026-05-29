#pragma once
#include <cmath>
#include <algorithm>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

struct SafetyLimiter : public Function
{
    float threshold = 0.98f;   // linear
    float release   = 0.002f;

    float env = 0.f;
    float gain = 1.f;

    inline float zap(float x)
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    float process(float x) override
    {
        float a = std::fabs(x);

        // envelope follower
        env += (a - env) * 0.01f;

        // compute target gain
        float target = 1.f;
        if (env > threshold)
            target = threshold / (env + 1e-12f);

        // smooth gain change
        gain += (target - gain) * release;

        return zap(x * gain);
    }
};
