#pragma once
#include <cmath>
#include <algorithm>
#include <random>

struct FinalOutputStage: public Function
{

    // Soft clip amount (0..1)
    float clipAmount = 0.5f;
    float clipLP = 0.5f;

    // Dither amount (in LSBs)
    float ditherAmount = 1.0f; // 1 LSB by default

    // State
    std::mt19937 rng { 0x12345678 };
    std::uniform_real_distribution<float> uni { -1.f, 1.f };

    inline float zap(float x)
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    inline float softClip(float x)
    {
        return x / (1.f + std::fabs(x));
    }

    // Triangular PDF dither
    inline float tpdf()
    {
        return (uni(rng) + uni(rng)) * 0.5f;
    }

    float process(float x)
    {
        // smooth clip control
        clipLP += 0.002f * (clipAmount - clipLP);

        // soft clip
        x = softClip(x * (1.f + clipLP * 4.f));

        // TPDF dither (assuming float->16/24bit later)
        float dither = tpdf() * ditherAmount * (1.f / 32768.f);
        x += dither;

        return zap(x);
    }
};
