#pragma once
#include <cmath>
#include <algorithm>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

struct LoudnessCompensator : public Function
{
    float target = 0.25f;   // target RMS
    float rms = 0.f;
    float gain = 1.f;

    inline float zap(float x)
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    float process(float x) override
    {
        rms += (x * x - rms) * 0.001f;
        float cur = std::sqrt(rms) + 1e-12f;

        float targetGain = target / cur;
        gain += (targetGain - gain) * 0.001f;

        return zap(x * gain);
    }
};
