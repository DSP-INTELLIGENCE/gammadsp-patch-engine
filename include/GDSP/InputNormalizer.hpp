struct InputNormalizer
{
    // target RMS level
    float target = 0.25f;

    // smoothing
    float attack  = 0.01f;
    float release = 0.002f;

    // state
    float env = 0.f;
    float gain = 1.f;

    inline float zap(float x)
    {
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    float process(float x)
    {
        float a = fabsf(x);
        env += (a - env) * ((a > env) ? attack : release);

        float desired = target / (env + 1e-9f);

        // smooth gain
        gain += 0.001f * (desired - gain);

        return zap(x * gain);
    }
};
