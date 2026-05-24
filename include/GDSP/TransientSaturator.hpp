#pragma once
#include <cmath>
#include <algorithm>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

struct TransientSaturator : public Function
{
    // user control 0..1
    float amount = 0.5f;
    float amountLP = 0.5f;

    // envelope follower
    float env = 0.f;

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
        // smooth control
        amountLP += 0.002f * (amount - amountLP);

        // envelope follower (fast attack, slow release)
        float a = std::fabs(x);
        env += (a - env) * ((a > env) ? 0.15f : 0.01f);

        // isolate transient energy
        float transient = a - env;

        // dynamic saturation on transient only
        float shaped = sat(x * (1.f + transient * 6.f * amountLP));

        // blend: preserve body, enhance attack
        float y = x + (shaped - x) * (transient * 4.f * amountLP);

        return zap(y);
    }
};
