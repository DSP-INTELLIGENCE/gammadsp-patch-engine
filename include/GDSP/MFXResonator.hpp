#pragma once
#include "GDSP_ModDelay.hpp"

class ModResonator : public ModDelay
{
public:
    ModResonator()
    {
        setFreq(220.f);   // default A3
        setDecay(0.96f);
        setMix(1.0f);
        setDamping(0.5f);
        setDrive(0.1f);
    }

    void setFreq(float hz)
    {
        freq = std::max(10.f, hz);
        setDelay(1.0f / freq);
    }

    void setDecay(float d)
    {
        decay = std::clamp(d, 0.f, 0.999f);
        setFM(decay);
    }

    void setMix(float m) { setMM(std::clamp(m, 0.f, 1.f)); }

    void setDamping(float d)
    {
        damping = std::clamp(d, 0.f, 1.f);
        float cutoff = 200.f + damping * 10000.f;
        m_lpf.setFreq(cutoff);
    }

    void setDrive(float d) { drive = std::max(0.f, d); }

    float process(float input) override
    {
        // Inject input as excitation
        float d = _delay.read();

        float fb = d * fm.process();
        fb = m_lpf.process(fb);
        fb = softClip(fb, drive);

        _delay.write(input + fb);

        return d * mm.process();
    }

private:
    float freq = 220.f;
    float decay = 0.96f;
    float damping = 0.5f;
    float drive = 0.1f;

    OnePole m_lpf;
};
