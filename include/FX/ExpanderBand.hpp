#pragma once
#include <cmath>
#include "EnvFollow.hpp"

// ------------------------------------------------------------
// Downward expander band (ratio < ∞)
// ------------------------------------------------------------
struct ExpanderBand
{
    // -------- Parameters --------
    float threshold = 0.05f;   // linear amplitude
    float ratio     = 2.0f;    // >1.0  (2:1, 4:1, etc)
    float makeupDB  = 0.0f;
    bool  bypass    = false;

    // -------- Detector --------
    EnvFollow env;

    float process(float x)
    {
        if (bypass)
            return x;

        float level = env.process(std::abs(x));

        float gain = 1.0f;

        if (level < threshold)
        {
            // downward expansion (linear-domain version)
            float over = level / threshold;     // in (0,1)
            gain = std::pow(over, ratio - 1.0f);
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
