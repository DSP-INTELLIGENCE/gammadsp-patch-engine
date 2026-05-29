#pragma once
#include "GDSP_ModDelay.hpp"
#include "GDSP_Filters.hpp"

class ModSlapBack : public ModDelay
{
public:

    ModSlapBack()
    {
        // Core slap timing
        setDelay(0.090f);      // 90 ms
        setFM(0.0f);          // essentially no feedback
        setMM(0.45f);         // moderate wetness
        setAM(1.0f);

        // Subtle movement
        setDepth(0.0012f);    // 0.12% delay movement
        setLFO(0.3f);         // slow drift

        // Flavor shaping
        setTone(0.65f);       // slight HF loss
        setDrive(0.15f);      // gentle saturation
    }

    void setTime(float t)    { setDelay(std::clamp(t, 0.04f, 0.18f)); }
    void setMix(float m)     { setMM(std::clamp(m, 0.0f, 0.7f)); }
    void setFeedback(float f){ setFM(std::clamp(f, 0.0f, 0.15f)); }

    void setLFO(float r)     { m_lfo.set(r); }
    void setDepth(float d)   { m_depth.set(d); }

    
    void setDrive(float d)   { m_drive = std::max(d, 0.0f); }

    float process(float input) override
    {
        float mod = m_lfo.process() * m_depth.process();
        setDM(mod);

        float y = update(input);

        // Slapback tone shaping
        y = m_lpf.process(y);
        y = softClip(y, m_drive * m_driveComp);

        return y;
    }

    void setTone(float t)
    {
        m_tone = std::clamp(t, 0.0f, 1.0f);
        updateTone();
    }

    void updateTone()
    {
        // Map 0..1 tone → cutoff 1.5 kHz .. 12 kHz
        float cutoff = 1500.0f + m_tone * (12000.0f - 1500.0f);
        m_lpf.setFreq(cutoff);

        // Darker tone = slightly more drive
        m_driveComp = 1.0f + (1.0f - m_tone) * 0.6f;
    }

private:
    Modulator m_lfo;
    Modulator m_depth;

    // simple one-pole LPF like TapeEcho
    OnePole   m_lpf;

    float m_driveComp = 1.0f;
    float m_tone  = 0.65f;
    float m_drive = 0.15f;
};


