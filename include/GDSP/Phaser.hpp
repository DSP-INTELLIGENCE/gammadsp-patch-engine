#pragma once
#include "GDSP_ModAllPass1.hpp"

class Phaser : public Function
{
public:
    Phaser()
    {
        setRate(0.2f);
        setDepth(0.8f);
        setFeedback(0.4f);
        setMix(0.6f);
        setAM(1.0f);

        // Base center frequencies for each stage
        m_ap[0].setCutoff(300.0f);
        m_ap[1].setCutoff(700.0f);
        m_ap[2].setCutoff(1400.0f);
        m_ap[3].setCutoff(2800.0f);
    }

    void setRate(float r)     { m_rate.set(r); }
    void setDepth(float d)    { m_depth = std::clamp(d, 0.f, 1.f); }
    void setFeedback(float f) { m_fb = std::clamp(f, -0.99f, 0.99f); }
    void setMix(float m)      { m_mix = std::clamp(m, 0.f, 1.f); }
    void setAM(float a)       { m_am.set(a); }

    float process(float input) override
    {
        float lfo = m_rate.process();
        float sig = input + m_last * m_fb;

        // Phase network
        for (int i = 0; i < 4; ++i)
        {
            float base = m_base[i];
            float fc   = base * (1.0f + lfo * m_depth);
            m_ap[i].setCutoff(fc);
            sig = m_ap[i].process(sig);
        }

        m_last = sig;

        float out = input * (1.0f - m_mix) + sig * m_mix;
        return out * m_am.process();
    }

private:
    ModAllPass1 m_ap[4];
    float       m_base[4] = { 300.f, 700.f, 1400.f, 2800.f };

    Modulator   m_rate;
    Modulator   m_am;

    float m_depth = 0.8f;
    float m_fb    = 0.4f;
    float m_mix   = 0.6f;
    float m_last  = 0.0f;
};
