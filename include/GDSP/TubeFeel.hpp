#pragma once
#pragma once
#include "OnePole.hpp"
#include "Biquad.hpp"
#include "EnvFollow.hpp"

#pragma once

class TubeFeel
{
public:
    TubeFeel()
    : env(12.0f) {}   // ~80 ms default response

    void prepare()
    {
        env.reset();
    }

    void setAmount(float a)
    {
        amount = std::clamp(a, 0.f, 1.f);
    }

    void setSpeed(float seconds)
    {
        env.setLag(seconds);
    }

    inline void apply(float input,
                      float& drive,
                      float& bias,
                      float& knee)
    {
        float e = env.process(std::fabs(input));

        // --- Drive compression ---
        drive *= (1.f - e * 0.45f * amount);

        // --- Bias shift (even harmonics & bloom) ---
        bias += e * 0.12f * amount;

        // --- Knee softening (smoother clipping when pushed) ---
        knee += e * 0.7f * amount;
    }

private:
    float amount = 0.f;
    EnvFollow env;
};
