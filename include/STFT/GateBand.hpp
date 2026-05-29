#pragma once
#include <cmath>
#include "EnvFollow.hpp"

// ------------------------------------------------------------
// Hard noise gate band
// Signal is muted when envelope falls below threshold
// ------------------------------------------------------------
struct GateBand
{
    // -------- Parameters --------
    float threshold = 0.05f;   // linear amplitude
    float makeupDB = 0.0f;
    bool  bypass   = false;

    // -------- Detector --------
    EnvFollow env;

    float process(float x)
    {
        if (bypass)
            return x;

        float level = env.process(std::abs(x));

        float gain = (level >= threshold) ? 1.0f : 0.0f;
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
