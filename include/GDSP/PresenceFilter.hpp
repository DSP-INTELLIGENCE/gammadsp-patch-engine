#pragma once
#include <cmath>
#include <algorithm>
#include "Engine.hpp"
#include "Parameters.hpp"

struct PresenceFilter : public Function
{
    // user control 0..1
    float amount = 0.5f;
    float amountLP = 0.5f;

    // filters
    TPT1Pole airHP;
    TPT1Pole airLP;

    float sr = 44100.f;

    void prepare(float sampleRate)
    {
        sr = sampleRate;
        airHP.setHPCut(3500.f, sr);
        airLP.setLPCut(15000.f, sr);
    }

    void reset()
    {
        airHP.reset();
        airLP.reset();
        amountLP = amount;
    }

    inline float zap(float x)
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    float process(float x) override
    {
        // smooth control
        amountLP += 0.002f * (amount - amountLP);

        // extract "air band"
        float air = airLP.processLP(airHP.processHP(x));

        // gentle presence lift
        x += air * (amountLP * 0.6f);

        return zap(x);
    }
};
