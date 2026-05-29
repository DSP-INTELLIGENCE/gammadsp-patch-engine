#pragma once
#include <cmath>
#include <algorithm>

class SpectralEventHorizon: public Function
{
public:
    SpectralEventHorizon()
    {
        setFeedback(0.6f);
        setBaseShift(20.f);
        setWidth(80.f);
        setRate(0.05f);
        setTilt(0.5f);
        setMix(0.7f);
    }

    // ---------- Musical Controls ----------
    void setFeedback(float f)   { feedback = std::clamp(f, 0.f, 0.98f); }
    void setBaseShift(float hz) { baseShift = std::max(0.f, hz); }
    void setWidth(float hz)     { width = std::max(0.f, hz); }
    void setRate(float hz)      { rate  = hz; m_rate.set(std::max(0.001f, hz)); }
    void setTilt(float t)       { tilt  = std::clamp(t, 0.f, 1.f); }
    void setMix(float m)        { mix   = std::clamp(m, 0.f, 1.f); }

    void reset()
    {
        phase = 0.f;
        last = 0.f;
        filter.reset();
    }

    // ---------- DSP ----------
    float process(float x)
    {
        // --- modulation phase ---
        float r = m_rate.process(rate);
        phase += r / (float)gam::sampleRate();
        phase -= std::floor(phase);

        // --- slow motion control ---
        float lfo = std::sin(phase * 6.28318530718f);
        float ctrl = 0.5f * (1.f + lfo);

        // --- dynamic frequency shift ---
        float shiftHz = m_shift.process(baseShift) + ctrl * m_width.process(width);
        shiftHz = std::min(shiftHz, 0.45f * (float)gam::sampleRate());
        shifter.freq(shiftHz);

        // --- feedback injection ---
        float fb = last * m_feedback.process(feedback);
        fb = fb / (1.f + std::abs(fb));   // soft energy limit

        float in = x + fb;

        // --- spectral processing ---
        float y = shifter(in);
        y = filter(y);

        // --- store feedback state ---
        last = y;

        // --- wet / dry ---
        float a = m_mix.process(mix) * 1.57079632679f;
        return x * std::cos(a) + y * std::sin(a);
    }

    // ---------- Modulation ----------
    Modulator m_rate;
    Modulator m_feedback;
    Modulator m_shift;
    Modulator m_width;
    Modulator m_tilt;
    Modulator m_mix;

private:
    // ---------- Spectral Core ----------
    gam::FreqShift<double> shifter;
    gam::Biquad<double>    filter { 1000.0, 0.707, gam::LOW_PASS };

    

    // ---------- Parameters ----------
    float rate      = 0.1f;
    float feedback  = 0.6f;
    float baseShift = 20.f;
    float width     = 80.f;
    float tilt      = 0.5f;     // reserved for filter morph
    float mix       = 0.7f;

    // ---------- State ----------
    float phase = 0.f;
    float last  = 0.f;
};
