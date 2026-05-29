#pragma once
#include <cmath>
#include "EnvFollow.hpp"

// ------------------------------------------------------------
// Compressor / Expander band
// ratio > 1  → compression
// ratio < 1  → expansion
// ------------------------------------------------------------
struct CompressorBand
{
    // -------- Parameters --------
    float threshold = 0.1f;   // linear amplitude
    float ratio     = 2.0f;   // >1 comp, <1 exp
    float makeupDB  = 0.0f;
    bool  bypass    = false;

    // -------- Detector --------
    EnvFollow env;

    float process(float x)
    {
        if (bypass)
            return x;

        float level = env.process(std::abs(x));

        float gainDB = 0.0f;

        if (ratio > 1.0f)
        {
            // ---- Compression ----
            if (level > threshold)
            {
                float over = level - threshold;
                gainDB = -over * (1.0f - 1.0f / ratio);
            }
        }
        else if (ratio < 1.0f)
        {
            // ---- Expansion ----
            if (level < threshold)
            {
                float under = threshold - level;
                gainDB = under * (1.0f - ratio);
            }
        }

        float gain = dbToLin(gainDB + makeupDB);
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
