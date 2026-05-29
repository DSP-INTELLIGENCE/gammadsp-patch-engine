#pragma once
#include "GDSP_ModDelay.hpp"
#include "GDSP_Filters.hpp"

class ModDoubler : public ModDelay
{
public:
    ModDoubler()
    {
        setDelay(0.018f);    // 18 ms
        setFM(0.02f);       // extremely low feedback
        setMM(0.5f);        // equal blend
        setAM(1.0f);

        setRate(0.35f);
        setDepth(0.0012f);  // micro drift

        setTone(0.6f);
        setDrive(0.08f);
    }

    void setTime(float t)    { setDelay(std::clamp(t, 0.01f, 0.03f)); }
    void setMix(float m)     { setMM(std::clamp(m, 0.2f, 0.8f)); }

    void setRate(float r)    { m_lfo.set(r); }
    void setDepth(float d)   { m_depth.set(d); }

    void setTone(float t)    { m_tone = std::clamp(t, 0.0f, 1.0f); }
    void setDrive(float d)   { m_drive = std::max(0.0f, d); }

    float process(float x) override
    {
        // micro timing drift
        float drift = m_lfo.process() * m_depth.process();
        setDM(drift);

        float y = update(x);

        // slight tonal difference between voices
        y = m_lpf.process(y);
        y = softClip(y, m_drive);

        return y;
    }

private:
    Modulator m_lfo, m_depth;

    OnePole m_lpf;

    float m_tone  = 0.6f;
    float m_drive = 0.08f;
};






