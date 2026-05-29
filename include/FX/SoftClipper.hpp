#pragma once

struct SoftClipper
{
    // Controls
    float amount = 1.f;     // strength of clipping
    float knee   = 1.f;     // 0.5 → soft, 2+ → hard
    float asym   = 0.f;     // even harmonic bias
    float trim   = 1.f;

    // Smoothing
    float amtLP = 1.f;
    float kneeLP = 1.f;
    float asymLP = 0.f;

    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    // Core transfer
    inline float clipCore(float x)
    {
        // soft knee
        float y = x * kneeLP;
        y = y / (1.f + fabsf(y));

        // asymmetry
        y += asymLP * y * y;

        return y;
    }

    // Loudness compensation
    inline float loudComp() const {
        return 1.f / (1.f + 0.25f * amtLP);
    }

    inline float process(float x)
    {
        // smooth controls
        amtLP  += 0.002f * (amount - amtLP);
        kneeLP += 0.002f * (knee   - kneeLP);
        asymLP += 0.002f * (asym   - asymLP);

        // drive into clipper
        float y = x * amtLP;

        // nonlinear shaping
        y = clipCore(y);

        // level control
        y *= trim * loudComp();

        return zap(y);
    }
};
