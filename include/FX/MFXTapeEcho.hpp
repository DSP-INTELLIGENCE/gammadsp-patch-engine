#pragma once
#include "GDSP_ModDelay.hpp"
#include "GDSP_Filters.hpp"

class ModTapeEcho : public ModDelay
{
public:
    ModTapeEcho()
    {
        setDelay(0.38f);      // base tape time
        setFM(0.72f);
        setMM(0.6f);
        setAM(1.0f);

        setRate(0.12f);      // slow tape wobble
        setDepth(0.0035f);

        setTone(0.45f);
        setDrive(0.3f);
        setNoise(0.01f);

        setSpacing(0.6f, 1.0f, 1.4f);   // classic 3-head pattern
    }

    void setRate(float r)  { m_lfo.set(r); }
    void setDepth(float d){ m_depth.set(d); }

    void setSpacing(float a, float b, float c)
    {
        m_heads[0] = a;
        m_heads[1] = b;
        m_heads[2] = c;
    }

    void setTone(float t)
    {
        m_tone = std::clamp(t, 0.f, 1.f);
        float cutoff = 900.f + m_tone * 7000.f;
        m_lpf.setFreq(cutoff);
    }

    void setDrive(float d) { m_drive = d; }
    void setNoise(float n) { m_noise = n; }

    float process(float input) override
    {
        float wobble = m_lfo.process() * m_depth.process();
        setDM(wobble);

        float base = getDelay();
        float sum  = 0.f;

        for(int i=0;i<3;i++)
        {
            _delay.setDelay(base * m_heads[i] * (1.f + wobble));
            sum += _delay.read();
        }

        sum *= 0.33f;

        // tape coloration
        sum = m_lpf.process(sum);
        sum = softClip(sum, m_drive);

        if(m_noise > 0.f)
            sum += (randf()*2.f-1.f) * m_noise;

        // feedback path
        float fb = std::clamp(fm.process(), 0.f, 0.98f);
        _delay.write(input + sum * fb);

        float mix = std::clamp(mm.process(), 0.f, 1.f);
        return input * (1.f - mix) + sum * mix;
    }

private:
    Modulator m_lfo, m_depth;
    OnePole   m_lpf;

    float m_heads[3] { 0.6f, 1.0f, 1.4f };

    float m_tone  = 0.45f;
    float m_drive = 0.3f;
    float m_noise = 0.01f;
};



