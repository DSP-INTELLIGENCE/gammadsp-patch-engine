#pragma once
#include <vector>
#include <algorithm>
#include "GDSP_ModAllPass1.hpp"
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

class PhaseShifter : public Function
{
public:
    PhaseShifter(int stages = 4)
    : baseFreq(500.f),
      baseFB(0.35f),
      baseMix(0.6f)
    {
        setRate(0.3f);
        setDepth(0.7f);
        setIM(1.f);
        setAM(1.f);

        for(int i = 0; i < stages; ++i)
            chain.emplace_back(baseFreq);
    }

    // ---------- Musical controls ----------
    void setRate(float r)     { rate.set(std::max(0.01f, r)); }
    void setDepth(float d)    { depth = std::clamp(d, 0.f, 1.f); }
    void setFeedback(float f) { baseFB = std::clamp(f, -0.9f, 0.9f); }
    void setMix(float m)      { baseMix = std::clamp(m, 0.f, 1.f); }

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float x) override
    {
        // LFO
        float r = rate.process();
        phase += r / (float)gam::sampleRate();
        if(phase >= 1.f) phase -= 1.f;

        float lfo = std::sin(phase * 6.2831853f);

        float cutoff = baseFreq * std::pow(2.f, lfo * depth * 2.f);
        cutoff = std::clamp(cutoff, 50.f, 0.45f * (float)gam::sampleRate());

        float in = x * im.process() + last * baseFB;

        for(auto& s : chain)
        {
            s.setCutoff(cutoff);
            in = s.process(in);
        }

        float out = x * (1.f - baseMix) + in * baseMix;
        out *= am.process();

        last = in;
        return out;
    }

private:
    std::vector<ModAllPass1> chain;

    float baseFreq;
    float baseFB;
    float baseMix;

    Modulator rate;
    float depth;

    Modulator im, am;

    float phase = 0.f;
    float last  = 0.f;
};
