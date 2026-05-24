#pragma once
#include <cmath>
#include "GDSP_Parameters.hpp"

class ModTremolo : public Function
{
public:
    ModTremolo()
    {
        setRate(4.0f);
        setDepth(0.7f);
        setShape(0.0f);
        setMix(1.0f);
    }

    void setRate(float r)  { m_rate.set(std::max(0.01f, r)); }
    void setDepth(float d) { m_depth.set(std::clamp(d, 0.f, 1.f)); }
    void setShape(float s) { m_shape = std::clamp(s, 0.f, 1.f); }
    void setMix(float m)   { m_mix   = std::clamp(m, 0.f, 1.f); }

    float process(float x) override
    {
        float rate  = m_rate.process();
        float depth = m_depth.process();

        // integrate phase
        phase += rate / (float)gam::sampleRate();
        if (phase >= 1.f) phase -= 1.f;

        float s = std::sin(phase * 6.2831853f);

        // sine → square morph
        float shaped = s * (1.f - m_shape) + std::copysign(1.f, s) * m_shape;

        // depth: 0 → unity, 1 → full tremolo
        float mod = (1.f - depth) + depth * (0.5f + 0.5f * shaped);

        float y = x * mod;
        return x * (1.f - m_mix) + y * m_mix;
    }

private:
    Modulator m_rate, m_depth;

    float m_shape = 0.f;
    float m_mix   = 1.f;
    float phase   = 0.f;
};
