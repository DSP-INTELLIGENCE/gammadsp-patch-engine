struct AnalogDrift
{
    // Controls
    float amount = 0.001f;     // overall drift depth
    float speed  = 0.00005f;   // how fast drift moves

    // Internal state
    float t = 0.f;
    float x1 = 0.f, x2 = 0.f;

    // Smoothing
    float amtLP = 0.001f;
    float spdLP = 0.00005f;

    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    // Cheap smooth noise
    inline float noise(){
        t += spdLP;
        return std::sin(t * 1.37f) + 0.5f * std::sin(t * 0.73f);
    }

    inline float process()
    {
        // smooth controls
        amtLP += 0.0005f * (amount - amtLP);
        spdLP += 0.0005f * (speed  - spdLP);

        // correlated random drift
        float n = noise();
        x1 += 0.001f * (n - x1);
        x2 += 0.0003f * (x1 - x2);

        float d = x2 * amtLP;

        return zap(d);
    }
};
