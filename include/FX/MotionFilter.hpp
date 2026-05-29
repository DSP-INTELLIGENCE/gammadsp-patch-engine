#pragma once
#include <algorithm>
#include <cmath>
#include "Engine.hpp"
#include "Parameters.hpp"
#include "Biquad.hpp"

class MotionFilter : public Function
{
public:
    MotionFilter()
    {
        filter.setType(gam::FilterType::BAND_PASS);
        setBaseCutoff(800.f);
        setResonance(0.7f);
        setRate(0.5f);
        setDepth(0.7f);
        setEnvAmount(0.4f);
        setMix(1.0f);
        setEnvTime(0.05f);  // 50 ms default
    }

    void setBaseCutoff(float f) { baseCutoff = std::max(40.f, f); }
    void setResonance(float r)  { resonance = std::clamp(r, 0.1f, 2.f); }

    void setRate(float r)       { rate.set(std::max(0.01f, r)); }
    void setDepth(float d)      { depth = std::clamp(d, 0.f, 1.f); }
    void setEnvAmount(float e)  { envAmt = std::clamp(e, 0.f, 1.f); }

    void setEnvTime(float t)    { envCoef = std::exp(-1.0f / (t * gam::sampleRate())); }

    void setMix(float m)        { mix = std::clamp(m, 0.f, 1.f); }

    float process(float x) override
    {
        // Envelope follower
        float level = std::fabs(x);
        env = level + envCoef * (env - level);

        // LFO
        float r = rate.process();
        phase += r / (float)gam::sampleRate();
        if (phase >= 1.f) phase -= 1.f;

        float lfo = std::sin(phase * 6.2831853f);

        // Modulated cutoff
        float mod = lfo * depth + env * envAmt;
        float cutoff = baseCutoff * std::pow(2.f, mod * 3.f);

        float sr = (float)gam::sampleRate();
        cutoff = std::clamp(cutoff, 40.f, 0.45f * sr);

        filter.freq(cutoff);
        filter.res(resonance);

        float y = filter(x);
        return x * (1.f - mix) + y * mix;
    }

    Biquad    filter;
    Modulator rate;

    float baseCutoff = 800.f;
    float resonance  = 0.7f;

    float depth  = 0.7f;
    float envAmt = 0.4f;

    float phase = 0.f;
    float env   = 0.f;
    float envCoef = 0.0f;

    float mix = 1.0f;
};
