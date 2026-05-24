struct OTA_ZDF_TPT1Pole
{
    float g  = 0.f;   // integrator coefficient
    float s  = 0.f;   // state
    float gm = 1.f;   // OTA transconductance

    static constexpr float kPi = 3.14159265358979323846f;

    void reset(float v = 0.f) { s = v; }

    void setCut(float fc, float fs)
    {
        fc = std::clamp(fc, 0.0001f * fs, 0.499f * fs);

        const float T  = 1.f / fs;
        const float wd = 2.f * kPi * fc;
        const float wa = (2.f / T) * std::tan(wd * T * 0.5f);

        g = std::min(wa * T * 0.5f, 1000.f);
    }

    static inline float ota(float x, float gm)
    {
        return std::tanh(x * gm);
    }

    inline float process(float x)
    {
        const float alpha = g / (1.f + g);

        // Implicit solve: y = s + alpha * ota(x - y)
        float y = s;   // excellent initial guess

        // 1–2 Picard iterations (stable for tanh)
        for (int i = 0; i < 2; ++i)
        {
            float iota = ota(x - y, gm);
            y = s + alpha * iota;
        }

        // TPT state update
        float v = y - s;
        s = y + v;

        return y;
    }
};
