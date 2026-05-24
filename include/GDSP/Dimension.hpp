#pragma once
#include "ModDelay.hpp"

class Dimension : public Function
{
public:
    Dimension()
    {
        m_baseL = 0.022f;
        m_baseR = 0.026f;

        L.setDelay(m_baseL);
        R.setDelay(m_baseR);

        L.setFM(0.0f);
        R.setFM(0.0f);

        L.setMIXM(1.0f);
        R.setMIXM(1.0f);

        setRate(0.25f);
        setDepth(0.0015f);
        setMix(0.5f);
        setWidth(1.0f);
        setTone(0.6f);
    }

    void setRate(float r)   { m_lfo.set(r); }
    void setDepth(float d)  { m_depth = d; }
    void setMix(float m)    { m_mix = std::clamp(m,0.f,1.f); }
    void setWidth(float w)  { m_width = std::clamp(w,0.f,1.f); }

    void setTone(float t)
    {
        m_tone = std::clamp(t, 0.f, 1.f);
        float cutoff = 2000.f + m_tone * 9000.f;
        lpfL.freq(cutoff);
        lpfR.freq(cutoff);
    }

    StereoSample process(float input)
    {
        float lfo = m_lfo.process();
        float mod = lfo * m_depth;

        // Stable base + perturbation
        L.setDelay(m_baseL * (1.0f - mod));
        R.setDelay(m_baseR * (1.0f + mod));

        float l = lpfL(L.process(input));
        float r = lpfR(R.process(input));

        float mid  = 0.5f * (l + r);
        float side = 0.5f * (l - r) * m_width;

        float outL = mid + side;
        float outR = mid - side;

        outL = input * (1.f - m_mix) + outL * m_mix;
        outR = input * (1.f - m_mix) + outR * m_mix;
        return StereoSample(outL,outR);        
    }

private:
    ModDelay L, R;
    Modulator m_lfo;

    float m_baseL, m_baseR;
    float m_depth = 0.0015f;
    float m_mix   = 0.5f;
    float m_width = 1.0f;
    float m_tone  = 0.6f;

    gam::OnePole<float> lpfL, lpfR;
};
