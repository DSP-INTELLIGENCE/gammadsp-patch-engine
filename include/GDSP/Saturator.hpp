struct Saturator
{
    // Controls
    float amount = 1.f;        // 0.5 → subtle, 4+ → aggressive
    float curve  = 1.f;        // 0.5 → soft, 2+ → sharp
    float asym   = 0.f;        // even harmonic bias
    float trim   = 1.f;

    // Smoothing
    float amtLP = 1.f;
    float curLP = 1.f;
    float asymLP = 0.f;

    // Denormal safety
    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    // Core transfer
    inline float satCore(float x)
    {
        // soft knee
        float y = x * (1.f + curLP);
        y = y / (1.f + fabsf(y));

        // even harmonics
        y += asymLP * y * y;

        // dynamic compression feel
        y = y / (1.f + 0.5f * fabsf(y));

        return y;
    }

    // Loudness normalization
    inline float loudComp() const {
        return 1.f / (1.f + 0.35f * amtLP);
    }

    inline float process(float x)
    {
        // smooth params
        amtLP  += 0.002f * (amount - amtLP);
        curLP  += 0.002f * (curve  - curLP);
        asymLP += 0.002f * (asym   - asymLP);

        // drive in
        float y = x * amtLP;

        // nonlinear shaping
        y = satCore(y);

        // output trim & level control
        y *= trim * loudComp();

        return zap(y);
    }
};
