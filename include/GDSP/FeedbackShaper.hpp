struct FeedbackShaper
{
    // Controls
    float amount = 1.f;        // feedback drive
    float limit  = 1.f;        // max feedback energy
    float asym   = 0.f;        // harmonic bias

    // Smoothing
    float amtLP = 1.f;
    float limLP = 1.f;
    float asymLP = 0.f;

    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    // Core feedback nonlinearity
    inline float shape(float x)
    {
        // dynamic compression of feedback energy
        float y = x * amtLP;
        y = y / (1.f + fabsf(y) / limLP);

        // asymmetry for harmonic richness
        y += asymLP * y * y;

        return y;
    }

    inline float process(float x)
    {
        // smooth controls
        amtLP  += 0.002f * (amount - amtLP);
        limLP  += 0.002f * (limit  - limLP);
        asymLP += 0.002f * (asym   - asymLP);

        float y = shape(x);

        return zap(y);
    }
};
