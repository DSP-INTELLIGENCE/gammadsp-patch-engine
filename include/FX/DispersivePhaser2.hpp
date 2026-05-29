#pragma once
#include "ModAllPass2.hpp"

class DispersivePhaser2 : public Function
{
public:
    DispersivePhaser2()
    {
        setRate(0.18f);
        setDepth(0.7f);
        setDispersion(0.6f);
        setFeedback(0.35f);
        setMix(0.65f);
        setAM(1.0f);

        // Log-spaced center freqs
        m_base[0] = 250.f;
        m_base[1] = 600.f;
        m_base[2] = 1400.f;
        m_base[3] = 3200.f;

        for (int i = 0; i < 4; ++i)
        {
            m_ap[i].setCutoff(m_base[i]);
            m_ap[i].setWidth(150.f);
        }
    }

    // -------- Controls --------

    void setRate(float r)        { m_rate.set(std::max(0.01f, r)); }
    void setDepth(float d)       { m_depth = std::clamp(d, 0.f, 1.f); }
    void setDispersion(float d)  { m_disp = std::clamp(d, 0.f, 1.f); }
    void setFeedback(float f)    { m_fb   = std::clamp(f, -0.98f, 0.98f); }
    void setMix(float m)         { m_mix  = std::clamp(m, 0.f, 1.f); }
    void setAM(float a)          { m_am.set(a); }

    // -------- Audio --------

    float process(float input) override
    {
        float lfo = m_rate.process();
        float sig = input + m_last * m_fb;

        for (int i = 0; i < 4; ++i)
        {
            float base = m_base[i];

            // Exponential frequency sweep
            float fc = base * std::pow(2.f, lfo * m_depth);

            // Width modulation = dispersion / group-delay color
            float width = 40.f + m_disp * (1.f + lfo) * 800.f;

            m_ap[i].setCutoff(fc);
            m_ap[i].setWidth(width);

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

    float m_depth = 0.7f;
    float m_disp  = 0.6f;
    float m_fb    = 0.35f;
    float m_mix   = 0.65f;

    float m_last = 0.f;
};
