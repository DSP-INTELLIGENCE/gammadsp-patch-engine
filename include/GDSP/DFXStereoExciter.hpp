#pragma once
#include "DFXCore.hpp"
#include <cmath>
#include <algorithm>

class DFXStereoExciter : public DFXCore
{
public:
    DFXStereoExciter()
    {
        setWet(1.0f);
        setDry(1.0f);
        setMix(1.0f);
    }

    // 0..1  how strong the harmonic generation is
    void setAmount(float a)
    {
        amount = std::clamp(a, 0.f, 1.f);
    }

    // where harmonics start being added (Hz)
    void setFocus(float hz)
    {
        focus = std::max(200.f, hz);
    }

    float process(float x) override
    {
        // mono → stereo domain
        float L = x;
        float R = x;

        // M/S
        float mid  = 0.5f * (L + R);
        float side = 0.5f * (L - R);

        // simple HF emphasis for side channel
        float h = side - lastSide;
        lastSide = side;

        // soft saturation on high band
        float k = 1.f + amount * 8.f;
        float sat = std::tanh(k * h) / std::tanh(k);

        // blend back
        side += sat * amount;

        float y = mid + side;

        return mixOut(x, y);
    }

private:
    float amount = 0.5f;
    float focus = 2500.f;

    float lastSide = 0.f;
};
