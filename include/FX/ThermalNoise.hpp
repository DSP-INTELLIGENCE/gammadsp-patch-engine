struct ThermalNoise
{
    float amount = 0.00002f;   // extremely subtle
    float color  = 0.2f;       // 0=white, 1=brownish
    float level  = 0.f;

    // noise filter
    float lp = 0.f;

    // RNG
    inline float rnd(){
        return 2.f * (float(rand()) / float(RAND_MAX)) - 1.f;
    }

    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    inline float process()
    {
        float n = rnd() * amount;

        // color the noise (RC thermal behavior)
        lp += color * (n - lp);

        // very slow fluctuation in intensity
        level += 0.00001f * (rnd() - level);

        return zap(lp * (1.f + 0.1f * level));
    }
};
