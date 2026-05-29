#pragma once
#include "ModDelay.hpp"
#include "Filters.hpp"

class MFXBBDDelay : public ModDelay
{
public:
    MFXBBD()
    {
        setDelay(0.25f);
        setFM(0.4f);
        setMM(0.5f);
        setAM(1.0f);

        setDepth(0.003f);
        setLFO(0.25f);

        setTone(0.4f);
        setDrive(0.25f);
        setNoise(0.01f);
    }

    void setLFO(float r)   { m_lfo.set(r); }
    void setDepth(float d){ m_depth.set(d); }

    void setTone(float t)  { m_tone = std::clamp(t,0.f,1.f); updateTone(); }
    void setDrive(float d){ m_drive = d; }
    void setNoise(float n){ m_noise = n; }

    float process(float x) override
    {
        float mod = m_lfo.process() * m_depth.process();
        setDM(mod);

        // BBD clocked sampling
        float bbd = clockSample(update(x));

        // BBD coloration
        bbd = m_lpf.process(bbd);
        bbd = softClip(bbd, m_drive);

        if(m_noise > 0)
            bbd += (randf()*2.f-1.f) * m_noise;

        return bbd;
    }

private:
    float clockSample(float x)
    {
        m_phase += m_clock;
        if(m_phase >= 1.f) {
            m_phase -= 1.f;
            m_hold = x;
        }
        return m_hold;
    }

    void updateTone()
    {
        float cutoff = 800.f + m_tone * 6000.f;
        m_lpf.setFreq(cutoff);
        m_clock = 0.005f + (1.f - m_tone) * 0.03f; // darker → slower clock
    }

    Modulator m_lfo, m_depth;
    OnePole m_lpf;

    float m_tone=0.4f, m_drive=0.25f, m_noise=0.01f;
    float m_phase=0.f, m_clock=0.02f, m_hold=0.f;
};



