#pragma once
#include "GDSP_ModDelay.hpp"

class ModShimmerDelay : public ModDelay
{
public:
    ModShimmerDelay()
    {
        setDelay(0.55f);
        setFM(0.65f);
        setMM(0.7f);
        setAM(1.0f);

        setRate(0.10f);
        setDepth(0.002f);

        setTone(0.55f);
        setDrive(0.25f);

        setPitch(12.0f);   // +12 semitones shimmer
    }

    void setRate(float r)    { m_lfo.set(r); }
    void setDepth(float d)   { m_depth.set(d); }

    void setPitch(float st)  { m_pitchST = st; }

    void setTone(float t)
    {
        m_tone = std::clamp(t, 0.f, 1.f);
        float cutoff = 1200.f + m_tone * 8000.f;
        m_lpf.setFreq(cutoff);
    }

    void setDrive(float d) { m_drive = std::max(0.f, d); }

    float process(float input) override
    {
        // time modulation
        float mod = m_lfo.process() * m_depth.process();
        setDM(mod);

        // read current delayed signal
        float d = _delay.read();

        // pitch shift the feedback signal
        float p = m_pitch.process(d, m_pitchST);

        // diffuse & color
        p = m_diff.process(p);
        p = m_lpf.process(p);
        p = softClip(p, m_drive);

        // write feedback
        float fb = std::clamp(fm.process(), 0.f, 0.98f);
        _delay.write(input + p * fb);

        // wet/dry
        float mix = std::clamp(mm.process(), 0.f, 1.f);
        return input * (1.f - mix) + d * mix;
    }

private:
    Modulator m_lfo, m_depth;

    ModDiffuser m_diff;
    OnePole     m_lpf;

    float m_tone  = 0.55f;
    float m_drive = 0.25f;

    // Pitch shifter (any algorithm you already have / plan to use)
    PitchShifter m_pitch;
    float m_pitchST = 12.f;
};
