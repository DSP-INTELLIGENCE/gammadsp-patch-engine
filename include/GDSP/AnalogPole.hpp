#pragma once
#include "GDSP_Waveshaper.hpp"

struct AnalogPole
{
    float g = 0.f;
    float s = 0.f;

    // gm behaves like “drive into OTA”
    float gm = 1.f;

    Waveshaper ws;

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

    inline float zap(float x){
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    inline float nl(float x)
    {
        return ws.process(x);
    }

    inline float process(float x)
    {
        // gm drives the nonlinearity, but cutoff stays controlled by g
        const float i = nl((x - s) * gm);

        const float v = i * (g / (1.f + g));
        const float y = v + s;

        s = zap(y + v);
        return y;
    }
};

