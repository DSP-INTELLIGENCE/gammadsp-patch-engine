#pragma once
#include <cmath>
#include "EnvFollow.hpp"

// ------------------------------------------------------------
// Hard limiter band
// Gain is clamped so output never exceeds threshold
// ------------------------------------------------------------
struct LimiterBand
{
    // -------- Parameters --------
    float threshold = 0.1f;   // linear amplitude
    float makeupDB = 0.0f;
    bool  bypass   = false;

    // -------- Detector --------
    EnvFollow env;

    float process(float x)
    {
        if (bypass)
            return x;

        float level = env.process(std::abs(x));

        float gain = 1.0f;

        if (level > threshold)
        {
            // hard clamp gain
            gain = threshold / level;
        }

        gain *= dbToLin(makeupDB);
        return x * gain;
    }

    void reset()
    {
        env.reset();
    }

    static float dbToLin(float db)
    {
        return std::pow(10.0f, db / 20.0f);
    }
};
