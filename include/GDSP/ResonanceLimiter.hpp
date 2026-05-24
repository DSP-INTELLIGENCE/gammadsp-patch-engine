struct ResonanceLimiter
{
    // user controls
    float threshold = 0.9f;     // when compression begins
    float softness  = 0.6f;     // curve of compression
    float recovery  = 0.001f;   // return speed

    // state
    float env = 0.f;

    inline float zap(float x)
    {
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    float process(float x)
    {
        float a = fabsf(x);

        // envelope follower for feedback energy
        env += (a - env) * 0.01f;

        // compression law
        float gain = 1.f;
        if (env > threshold)
        {
            float excess = env - threshold;
            gain = 1.f / (1.f + excess / (softness + 1e-6f));
        }

        // slow relaxation
        env += (0.f - env) * recovery;

        return zap(x * gain);
    }
};
