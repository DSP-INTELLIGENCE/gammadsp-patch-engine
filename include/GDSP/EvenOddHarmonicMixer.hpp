#pragma once
#include <cmath>
#include <algorithm>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

struct EvenOddHarmonicMixer : public Function
{
    // 0..1
    float evenAmount = 0.5f;
    float oddAmount  = 0.5f;

    float evenLP = 0.5f;
    float oddLP  = 0.5f;

    inline float zap(float x)
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    inline float softSat(float x)
    {
        return x / (1.f + std::fabs(x));
    }

    float process(float x) override
    {
        // smooth controls
        evenLP += 0.002f * (evenAmount - evenLP);
        oddLP  += 0.002f * (oddAmount  - oddLP);

        // Even harmonics (asymmetry)
        float even = softSat(x + evenLP * x * x) - x;

        // Odd harmonics (symmetric drive)
        float odd  = softSat(x * (1.f + oddLP * 2.f)) - x;

        // Blend
        x += 0.5f * even + 0.5f * odd;

        return zap(x);
    }
};
