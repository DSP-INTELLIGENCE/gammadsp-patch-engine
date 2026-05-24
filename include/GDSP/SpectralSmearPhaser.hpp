#pragma once
#include "GDSP_ModAllPass2.hpp"

class SpectralSmearPhaser : public Function
{
public:
    SpectralSmearPhaser()
    {
        setRate(0.12f);
        setDepth(0.6f);
        setDispersion(0.7f);
        setSmear(0.8f);
        setFeedback(0.4f);
        setMix(0.7f);
        setAM(1.0f);

        setEnvTime(0.08f);

        m_base[0] = 220.f;
        m_base[1] = 520.f;
        m_base[2] = 1200.f;
        m_base[3] = 3000.f;

        for (int i = 0; i < 4; ++i)
        {
            m_ap[i].setCutoff(m_base[i]);
            m_ap[i].setWidth(120.f);
        }
    }

    // -------- Controls --------

    void setRate(float r)       { m_rate.set(std::max(0.01f, r)); }
    void setDepth(float d)      { m_depth = std::clamp(d, 0.f, 1.f); }
    void setDispersion(float d) { m_disp  = std::clamp(d, 0.f, 1.f); }
    void setSmear(float s)      { m_smear = std::clamp(s, 0.f, 1.f); }
    void setFeedback(float f)   { m_fb    = std::clamp(f, -0.98f, 0.98f); }
    void setMix(float m)        { m_mix   = std::clamp(m, 0.f, 1.f); }
    void setAM(float a)         { m_am.set(a); }

    void setEnvTime(float t)
    {
        m_envCoef = std::exp(-1.0f / (t * gam::sampleRate()));
    }

    // -------- Audio --------

    float process(float input) override
    {
        // Envelope follower
        float lvl = std::fabs(input);
        m_env = lvl + m_envCoef * (m_env - lvl);

        float lfo = m_rate.process();
        float sig = input + m_last * m_fb;

        for (int i = 0; i < 4; ++i)
        {
            float base = m_base[i];

            float fc = base * std::pow(2.f, lfo * m_depth);

            float smear = 40.f 
                        + m_disp * 600.f 
                        + m_env  * m_smear * 1200.f;

            m_ap[i].setCutoff(fc);
            m_ap[i].setWidth(smear);

            sig = m_ap[i].process(sig);
        }

        m_last = sig;

        float out = input * (1.f - m_mix) + sig * m_mix;
        return out * m_am.process();
    }

private:
    ModAllPass2 m_ap[4];

    float m_base[4];

    Modulator m_rate;
    Modulator m_am;

    float m_depth = 0.6f;
    float m_disp  = 0.7f;
    float m_smear = 0.8f;
    float m_fb    = 0.4f;
    float m_mix   = 0.7f;

    float m_last = 0.f;

    // Envelope
    float m_env     = 0.f;
    float m_envCoef = 0.f;
};
