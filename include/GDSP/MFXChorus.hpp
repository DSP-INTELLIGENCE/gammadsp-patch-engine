#pragma once
#include "GDSP_ModDelay.hpp"

class MFXChorus : public ModDelay
{
public:
    MFXChorus()
    {
        m_baseDelay = 0.020f;   // 20 ms
        setDelay(m_baseDelay);
        setFM(0.0f);
        setMIXM(0.6f);
        setAM(1.0f);
        setDepth(0.002f);
    }

    void setLFO(float r)   { m_lfo.set(r); }
    void setDepth(float d){ m_depth.set(d); }

    float process(float input) override
    {
        float lfo   = m_lfo.process();
        float depth = m_depth.process();

        // Modulate around a *stable base*
        float modDelay = m_baseDelay + lfo * depth;
        setDelay(modDelay);

        return ModDelay::process(input);
    }

private:
    float m_baseDelay = 0.02f;
    Modulator m_lfo;
    Modulator m_depth;
};
