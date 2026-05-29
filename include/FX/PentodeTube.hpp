struct PentodeTube
{
    // Controls
    float drive      = 1.5f;   // higher intrinsic gain
    float bias       = 0.0f;   // operating point
    float hardness   = 0.6f;   // knee hardness
    float asymmetry  = 0.4f;   // odd harmonic emphasis
    float screenSag  = 0.3f;   // screen grid compression

    // Internal state
    float env = 0.f;
    float memory = 0.f;

    inline float zap(float x)
    {
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    inline float hardSat(float x)
    {
        return x / sqrtf(1.f + x * x);
    }

    float process(float x)
    {
        // Pre-gain and bias
        x = x * drive + bias;

        // Envelope follower (screen grid effect)
        float a = fabsf(x);
        env += (a - env) * 0.02f;

        // Screen grid sag reduces gain dynamically
        float sag = 1.f / (1.f + screenSag * env);
        x *= sag;

        // Memory / hysteresis (less than triode, but present)
        memory += 0.01f * (x - memory);

        // Asymmetric shaping
        float pos = hardSat(x + asymmetry * memory);
        float neg = hardSat(x - hardness * memory);

        float y = 0.6f * pos + 0.4f * neg;

        return zap(y);
    }
};
