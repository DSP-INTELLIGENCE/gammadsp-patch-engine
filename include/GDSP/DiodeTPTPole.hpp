struct DiodeTPTPole
{
    float g = 0.f;
    float s = 0.f;
    float vt = 0.35f; // normalized “thermal voltage” (tune to taste)


    static constexpr float kPi = 3.14159265358979323846f;

    void reset(float v = 0.f) { s = v; }

    void setCut(float fc, float fs)
    {
        fc = std::clamp(fc, 1.f, 0.499f * fs);
        float T  = 1.f / fs;
        float wd = 2.f * kPi * fc;
        float wa = (2.f / T) * std::tan(wd * T * 0.5f);
        g = std::min(wa * T * 0.5f, 1000.f);
    }

    static inline float diode(float x, float vt)
    {
        vt = std::max(vt, 1e-6f);
        return std::tanh(x / vt);
    }

    inline float process(float x)
    {
        float i = diode(x - s, vt);

        float v = i * (g / (1.f + g));
        float y = v + s;
        s = y + v;
        return y;
    }
};
