#pragma once
#include <cmath>
#include <algorithm>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

struct HarmonicEnhancer : public Function
{
    // user control 0..1
    float amount = 0.3f;
    float amountLP = 0.3f;

    // optional emphasis on high-frequency transients
    TPT1Pole highHP;
    TPT1Pole highLP;

    float sr = 44100.f;

    void prepare(float sampleRate)
    {
        sr = sampleRate;
        highHP.setHPCut(4000.f, sr);
        highLP.setLPCut(16000.f, sr);
    }

    void reset()
    {
        highHP.reset();
        highLP.reset();
        amountLP = amount;
    }

    inline float zap(float x)
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    inline float sat(float x)
    {
        return x / (1.f + std::fabs(x));
    }

    float process(float x) override
    {
        // smooth amount
        amountLP += 0.002f * (amount - amountLP);

        // isolate upper band for harmonic generation
        float high = highLP.processLP(highHP.processHP(x));

        // generate subtle harmonics
        float h = sat(high * 2.f) - high;

        // blend harmonics back in
        x += h * (amountLP * 0.25f);

        return zap(x);
    }
};
