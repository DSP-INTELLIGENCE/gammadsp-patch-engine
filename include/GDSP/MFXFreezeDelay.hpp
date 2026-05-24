#pragma once
#include "GDSP_ModDelay.hpp"

class ModFreezeDelay : public ModDelay
{
public:
    ModFreezeDelay()
    {
        setDelay(0.6f);
        setFM(0.85f);
        setMM(0.7f);
        setAM(1.0f);

        setRate(0.08f);
        setDepth(0.002f);

        setTone(0.5f);
        setDrive(0.2f);

        setFreeze(false);
    }

    void setRate(float r)   { m_lfo.set(r); }
    void setDepth(float d)  { m_depth.set(d); }

    void setFreeze(bool f)  { m_freeze = f; }

    void setTone(float t)
    {
        m_tone = std::clamp(t, 0.f, 1.f);
        float cutoff = 1000.f + m_tone * 9000.f;
        m_lpf.setFreq(cutoff);
    }

    void setDrive(float d) { m_drive = std::max(0.f, d); }

    float process(float input) override
    {
        float mod = m_lfo.process() * m_depth.process();
        setDM(mod);

        float d = _delay.read();

        // When frozen, block new input entirely
        float in = m_freeze ? 0.0f : input;

        // Color the feedback
        float fbSig = m_lpf.process(d);
        fbSig = softClip(fbSig, m_drive);

        float fb = m_freeze ? 1.0f : std::clamp(fm.process(), 0.f, 0.98f);

        _delay.write(in + fbSig * fb);

        float mix = std::clamp(mm.process(), 0.f, 1.f);
        return input * (1.f - mix) + d * mix;
    }

private:
    Modulator m_lfo, m_depth;

    OnePole m_lpf;

    float m_tone  = 0.5f;
    float m_drive = 0.2f;
    bool  m_freeze = false;
};
