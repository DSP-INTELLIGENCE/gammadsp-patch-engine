#pragma once
#include <cmath>
#include <algorithm>
#include <Gamma/Filter.h>
#include "GDSP_Parameters.hpp"

class ModUniVibe : public Function
{
public:
    ModUniVibe()
    {
        setRate(0.8f);
        setDepth(0.85f);
        setMix(0.6f);
        setDrive(0.15f);
        setThrob(0.4f);
    }

    void setRate(float r)   { m_rate.set(std::max(0.01f, r)); }
    void setDepth(float d)  { m_depth = std::clamp(d, 0.f, 1.f); }
    void setMix(float m)    { m_mix = std::clamp(m, 0.f, 1.f); }
    void setDrive(float d)  { m_drive = std::max(0.f, d); }
    void setThrob(float t)  { m_throb = std::clamp(t, 0.f, 1.f); }

    float process(float input) override
    {
        // Preamp color
        float x = softClip(input, m_drive);

        // Integrate phase
        phase += m_rate.process() / (float)gam::sampleRate();
        if (phase >= 1.f) phase -= 1.f;

        // Optical-style LFO (warped sine)
        float s = std::sin(phase * 6.2831853f);
        float lfo = std::tanh(s * 1.5f);       // optical curvature
        lfo = (lfo + 1.f) * 0.5f;               // 0..1

        // Non-linear sweep mapping
        float sweep = std::pow(lfo, 1.7f);      // bias toward low end
        float a = 0.2f + sweep * m_depth * 0.6f;

        // 4-stage all-pass
        float y = x;
        for (int i = 0; i < 4; ++i)
            y = ap[i].process(y, a);

        // Optical amplitude throb
        float throb = 1.f - m_throb * (1.f - sweep);
        y *= throb;

        return input * (1.f - m_mix) + y * m_mix;
    }

private:
    gam::AllPass1<float> ap[4];
    Modulator m_rate;

    float m_depth = 0.85f;
    float m_mix   = 0.6f;
    float m_drive = 0.15f;
    float m_throb = 0.4f;
    float phase   = 0.f;
};

/*
If you want next, we can build:
• Rotary speaker from this
• Leslie simulator
• Dimension-D ensemble preset bank
*/