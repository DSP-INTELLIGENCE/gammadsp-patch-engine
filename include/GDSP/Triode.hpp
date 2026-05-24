#pragma once

struct Triode
{
    float mu  = 100.f;
    float kg1 = 1060.f;
    float kp  = 600.f;
    float ex  = 1.4f;
    float vct = 0.0f;

    inline float process(float vgk, float vpk)
    {
        float e = vgk + vct + vpk / mu;

        // Smooth cutoff
        e = std::max(e, 0.f);

        // Soft saturation of drive domain
        e = std::tanh(0.7f * e);

        // Plate resistance interaction
        float denom = 1.f + kp * std::max(0.05f, vpk);

        float ip = kg1 * std::pow(e, ex) / denom;

        // Physical clamp: current never negative
        return std::clamp(ip, 0.f, 10.f);
    }
};

