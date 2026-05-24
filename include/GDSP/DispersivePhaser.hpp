#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include "ModAllPass1.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"

class DispersivePhaser : public Function
{
public:
    DispersivePhaser(int stages = 10)
    : baseFreq(300.f),
      baseFB(0.65f),
      baseMix(0.7f),
      baseDispersion(0.5f)
    {
        setRate(0.2f);
        setDepth(0.9f);
        setEnvAmount(0.2f);
        setIM(1.f);
        setAM(1.f);

        for(int i = 0; i < stages; ++i)
            chain.emplace_back(baseFreq);
    }

    // -------- Controls --------
    void setRate(float r)        { rate.set(std::max(0.01f, r)); }
    void setDepth(float d)       { depth = std::clamp(d, 0.f, 1.f); }
    void setFeedback(float f)    { baseFB = std::clamp(f, -0.99f, 0.99f); }
    void setDispersion(float d)  { baseDispersion = std::clamp(d, 0.f, 1.f); }
    void setMix(float m)         { baseMix = std::clamp(m, 0.f, 1.f); }
    void setEnvAmount(float e)   { envAmt = std::clamp(e, 0.f, 1.f); }

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float x) override
    {
        // Envelope follower
        float level = std::fabs(x);
        env += 0.04f * (level - env);

        // LFO
        float r = rate.process();
        phase += r / (float)gam::sampleRate();
        if(phase >= 1.f) phase -= 1.f;

        float lfo = std::sin(phase * 6.2831853f);

        // Motion
        float motion = lfo * depth + env * envAmt;

        float center = baseFreq * std::pow(2.f, motion * 3.f);
        center = std::clamp(center, 40.f, 0.45f * (float)gam::sampleRate());

        float in = x * im.process() + last * baseFB;

        for(int i = 0; i < (int)chain.size(); ++i)
        {
            float offset = (i - chain.size() * 0.5f) / chain.size();
            float f = center * std::pow(2.f, offset * baseDispersion * 2.f);

            f = std::clamp(f, 40.f, 0.45f * (float)gam::sampleRate());

            chain[i].setCutoff(f);
            in = chain[i].process(in);
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
    float baseDispersion;

    Modulator rate;
    float depth;
    float envAmt;

    Modulator im, am;

    float phase = 0.f;
    float env   = 0.f;
    float last  = 0.f;
};
