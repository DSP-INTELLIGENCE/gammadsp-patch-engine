#pragma once
#include "GDSP_ModDelay.hpp"

class ModVibrato : public ModDelay
{
public:
    ModVibrato()
    {
        m_baseDelay = 0.005f;   // 5 ms
        setDelay(m_baseDelay);
        setFM(0.0f);
        setMIXM(1.0f);
        setAM(1.0f);
        setDepth(0.6f);
    }

    void setLFO(float v)   { m_lfo.set(v); }
    void setDepth(float d){ m_depth.set(d); }

    float process(float input) override
    {
        float lfo   = m_lfo.process();
        float depth = std::clamp(m_depth.process(), 0.0f, 1.0f);

        float modDelay = m_baseDelay * (1.0f + lfo * depth);
        setDelay(modDelay);

        return ModDelay::process(input);
    }

private:
    float m_baseDelay = 0.005f;
    Modulator m_lfo;
    Modulator m_depth;
};
