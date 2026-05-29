#pragma once
#include "GDSP_ModDelay.hpp"

class ModHaasWidener : public Function
{
public:
    ModHaasWidener()
    {
        delay.setDelay(0.012f);   // 12 ms default
        delay.setFM(0.0f);
        delay.setMM(1.0f);

        setWidth(1.0f);
        setMix(1.0f);
    }

    void setTime(float t)  { delay.setDelay(std::clamp(t, 0.001f, 0.03f)); }
    void setWidth(float w){ m_width = std::clamp(w,0.f,1.f); }
    void setMix(float m)  { m_mix = std::clamp(m,0.f,1.f); }

    std::pair<float,float> process(float input)
    {
        float R = delay.update(input);
        float L = input;

        // Width shaping
        float mid  = (L + R) * 0.5f;
        float side = (L - R) * 0.5f * m_width;

        float outL = mid + side;
        float outR = mid - side;

        outL = input * (1.f - m_mix) + outL * m_mix;
        outR = input * (1.f - m_mix) + outR * m_mix;

        return { outL, outR };
    }

private:
    ModDelay delay;
    float m_width = 1.0f;
    float m_mix   = 1.0f;
};

