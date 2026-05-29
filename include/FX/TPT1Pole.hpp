#pragma once
struct TPT1Pole
{
    float g = 0.f;
    float s = 0.f;
    float a = 0.f;

    static constexpr float kPi = 3.14159265358979323846f;

    TPT1Pole(float fc=1000.f) {
        reset();
        setCut(fc, gam::sampleRate());
    }

    void reset(float v = 0.f) { s = v; }

    void setCut(float fc, float fs)
    {
        fc = std::clamp(fc, 0.0001f * fs, 0.499f * fs);

        const float T  = 1.f / fs;
        const float wd = 2.f * kPi * fc;
        const float wa = (2.f / T) * std::tan(wd * T * 0.5f);

        g = wa * T * 0.5f;
    }
    
    inline float processLP(float x)
    {
        const float v = (x - s) * (g / (1.f + g));
        const float y = v + s;
        s = y + v;
        return y;
    }

    inline float processHP(float x)
    {
        return x - processLP(x);
    }
};
