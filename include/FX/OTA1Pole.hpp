#pragma once
#include "GDSP_Waveshaper.hpp"

struct OTA_TPT1Pole
{
    float g = 0.f;
    float s = 0.f;
    float gm = 1.f;

    static constexpr float kPi = 3.14159265358979323846f;

    void reset(float v = 0.f) { s = v; }

    void setCut(float fc, float fs)
    {
        fc = std::clamp(fc, 0.0001f * fs, 0.499f * fs);

        float T  = 1.f / fs;
        float wd = 2.f * kPi * fc;
        float wa = (2.f / T) * std::tan(wd * T * 0.5f);
        g = std::min(wa * T * 0.5f, 1000.f);
    }

    inline float zap(float x){
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    static inline float ota(float x, float gm){
        return std::tanh(x * gm);
    }

    inline float process(float x)
    {
        float i = ota(x - s, gm);

        float v = i * (g / (1.f + g));
        float y = v + s;

        s = zap(y + v);
        return y;
    }
};


