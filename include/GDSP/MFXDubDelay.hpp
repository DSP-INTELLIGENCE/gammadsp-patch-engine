#pragma once
#include "GDSP_ModDelay.hpp"
#include "GDSP_Filters.hpp"

class ModDubDelay : public ModDelay
{
public:
    ModDubDelay()
    {
        setDelay(0.45f);     // long base time
        setFM(0.75f);       // strong feedback
        setMM(0.65f);       // mostly wet
        setAM(1.0f);

        setDepth(0.004f);   // heavy drift
        setLFO(0.15f);      // slow wobble

        setTone(0.35f);     // dark
        setDrive(0.35f);    // crunchy
        setNoise(0.02f);    // tape hiss

        setFreeze(false);
    }

    void setTime(float t)     { setDelay(std::clamp(t, 0.1f, 1.5f)); }
    void setFeedback(float f) { setFM(std::clamp(f, 0.0f, 0.98f)); }
    void setMix(float m)      { setMM(std::clamp(m, 0.0f, 1.0f)); }

    void setLFO(float r)      { m_lfo.set(r); }
    void setDepth(float d)    { m_depth.set(d); }

    void setDrive(float d)    { m_drive = std::max(0.0f, d); }
    void setNoise(float n)    { m_noise = std::max(0.0f, n); }

    void setFreeze(bool f)    { m_freeze = f; }

    float process(float input) override
    {
        float mod = m_lfo.process() * m_depth.process();
        setDM(mod);

        float dry = input;

        // Read from delay
        float d = _delay.read();

        // Feedback with freeze option
        float fb = m_freeze ? 1.0f : std::clamp(fm.process(), 0.0f, 0.98f);

        fbSig = m_lpf.process(fbSig);
        fbSig = softClip(fbSig, m_drive * m_driveComp);

        // Hiss
        if (m_noise > 0.0f)
            fbSig += (randf() * 2.f - 1.f) * m_noise;

        _delay.setDelay(getDelay() * (1.0f + dm.process()));
        _delay.write(input + fbSig);

        float mix = std::clamp(mm.process(), 0.0f, 1.0f);
        float y = dry * (1.0f - mix) + d * mix;

        return y * am.process();
    }

    void updateTone()
    {
        // Tone 0..1 → cutoff 700 Hz .. 9 kHz
        float cutoff = 700.0f + m_tone * (9000.0f - 700.0f);
        m_lpf.setFreq(cutoff);

        // Darker tone = more saturation inside the loop
        m_driveComp = 1.0f + (1.0f - m_tone) * 1.2f;
    }

    void setTone(float t)
    {
        m_tone = std::clamp(t, 0.0f, 1.0f);
        updateTone();
    }

private:
    Modulator m_lfo, m_depth;

    OnePole m_lpf;

    float m_tone  = 0.35f;
    float m_drive = 0.35f;
    float m_noise = 0.02f;
    float m_driveComp = 1.0f;
    bool m_freeze = false;
};


