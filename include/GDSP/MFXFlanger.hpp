#pragma once
#include "GDSP_ModDelay.hpp"

class ModFlanger : public ModDelay
{
public:
    ModFlanger()
    {
        m_baseDelay = 0.002f;   // 2 ms
        setDelay(m_baseDelay);
        setFM(0.7f);
        setMIXM(0.5f);
        setAM(1.0f);
        setDepth(0.8f);
    }

    void setLFO(float v)   { m_lfo.set(v); }
    void setDepth(float d){ m_depth.set(d); }

    float process(float input) override
    {
        float lfo   = m_lfo.process();
        float depth = std::clamp(m_depth.process(), 0.0f, 1.0f);

        // Proper flanger modulation
        float modDelay = m_baseDelay * (1.0f + lfo * depth);
        setDelay(modDelay);

        return ModDelay::process(input);
    }

private:
    float m_baseDelay = 0.002f;
    Modulator m_lfo;
    Modulator m_depth;
};
