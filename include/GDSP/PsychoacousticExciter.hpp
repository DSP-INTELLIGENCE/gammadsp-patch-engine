#pragma once
#include <cmath>
#include <algorithm>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

struct PsychoacousticExciter : public Function
{
    // user control 0..1
    float amount = 0.3f;
    float amountLP = 0.3f;

    // band isolation
    TPT1Pole highHP;
    TPT1Pole highLP;

    // envelope follower
    float env = 0.f;

    float sr = 44100.f;

    void prepare(float sampleRate)
    {
        sr = sampleRate;
        highHP.setHPCut(5000.f, sr);
        highLP.setLPCut(16000.f, sr);
    }

    void reset()
    {
        highHP.reset();
        highLP.reset();
        env = 0.f;
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
        amountLP += 0.002f * (amount - amountLP);

        // isolate high band
        float high = highLP.processLP(highHP.processHP(x));

        // detect signal energy
        float a = std::fabs(high);
        env += (a - env) * 0.02f;

        // dynamic harmonic synthesis
        float drive = 1.f + env * 6.f * amountLP;
        float exc = sat(high * drive) - high;

        // blend excitation back in
        x += exc * (amountLP * 0.35f);

        return zap(x);
    }
};
