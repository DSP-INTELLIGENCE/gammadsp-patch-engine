#pragma once
#include <cmath>
#include "EnvFollow.hpp"

// ------------------------------------------------------------
// Transient shaper (attack / sustain)
// ------------------------------------------------------------
struct TransientShaperBand
{
    // -------- Parameters --------
    float attackGain  = 1.0f;   // >1 boosts attack, <1 reduces
    float sustainGain = 1.0f;   // >1 boosts sustain, <1 reduces
    bool  bypass      = false;

    // -------- Detectors --------
    EnvFollow fastEnv;   // short attack / short release
    EnvFollow slowEnv;   // long attack / long release

    float process(float x)
    {
        if (bypass)
            return x;

        float absx = std::abs(x);

        float fast = fastEnv.process(absx);
        float slow = slowEnv.process(absx);

        // transient energy (positive only)
        float transient = fast - slow;
        if (transient < 0.0f)
            transient = 0.0f;

        // weighting
        float gain =
            1.0f +
            transient * (attackGain  - 1.0f) +
            slow      * (sustainGain - 1.0f);

        return x * gain;
    }

    void reset()
    {
        fastEnv.reset();
        slowEnv.reset();
    }
};
