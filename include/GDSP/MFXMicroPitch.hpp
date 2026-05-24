#pragma once
#include "GDSP_ModDelay.hpp"

class ModMicroPitch : public Function
{
public:
    ModMicroPitch()
    {
        // H3000 classic defaults
        left.setDelay(0.015f);
        right.setDelay(0.017f);

        left.setFM(0.0f);
        right.setFM(0.0f);

        left.setMM(1.0f);
        right.setMM(1.0f);

        setRate(0.25f);
        setDepth(0.0015f);
        setWidth(1.0f);
        setMix(0.7f);
    }

    void setRate(float r)   { m_lfo.set(r); }
    void setDepth(float d)  { m_depth.set(d); }
    void setWidth(float w)  { m_width = std::clamp(w,0.f,1.f); }
    void setMix(float m)    { m_mix = std::clamp(m,0.f,1.f); }

    std::pair<float,float> process(float input)
    {
        float lfo = m_lfo.process();
        float detune = m_depth.process();

        // opposite modulation directions
        left.setDM(-lfo * detune);
        right.setDM(lfo * detune);

        float L = left.update(input);
        float R = right.update(input);

        // stereo width
        float mid = (L + R) * 0.5f;
        float side = (L - R) * 0.5f * m_width;

        float outL = mid + side;
        float outR = mid - side;

        // dry blend
        outL = input * (1.f - m_mix) + outL * m_mix;
        outR = input * (1.f - m_mix) + outR * m_mix;

        return { outL, outR };
    }

private:
    ModDelay left, right;
    Modulator m_lfo, m_depth;

    float m_width = 1.f;
    float m_mix = 0.7f;
};

