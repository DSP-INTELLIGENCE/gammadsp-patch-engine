#pragma once
#include <cmath>
#include <algorithm>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

struct AntiAliasFilter
{
    TPT1Pole lp1, lp2;
    float sr = 44100.f;

    void prepare(float sampleRate)
    {
        sr = sampleRate;
        // slightly below Nyquist to suppress foldback energy
        float cutoff = 0.45f * sr;
        lp1.setLPCut(cutoff, sr);
        lp2.setLPCut(cutoff, sr);
    }

    void reset()
    {
        lp1.reset();
        lp2.reset();
    }

    inline float process(float x)
    {
        // 2-pole lowpass
        x = lp1.processLP(x);
        x = lp2.processLP(x);
        return x;
    }
};
