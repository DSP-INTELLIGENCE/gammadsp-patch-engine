#pragma once
#include <vector>
#include <algorithm>
#include "GDSP_ModAllPass1.hpp"
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

class MultiStagePhaser : public Function
{
public:
    MultiStagePhaser(int stages = 8)
    : baseFreq(350.f),
      baseFB(0.6f),
      baseMix(0.7f)
    {
        setRate(0.2f);
        setDepth(0.85f);
        setEnvAmount(0.3f);
        setIM(1.f);
        setAM(1.f);

        for(int i = 0; i < stages; ++i)
            chain.emplace_back(baseFreq);
    }

    // ---------- Musical Controls ----------
    void setRate(float r)     { rate.set(std::max(0.01f, r)); }
    void setDepth(float d)    { depth = std::clamp(d, 0.f, 1.f); }
    void setFeedback(float f) { baseFB = std::clamp(f, -0.99f, 0.99f); }
    void setMix(float m)      { baseMix = std::clamp(m, 0.f, 1.f); }
    void setEnvAmount(float e){ envAmt = std::clamp(e, 0.f, 1.f); }

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float x) override
    {
        // Envelope follower
        float level = std::fabs(x);
        env += 0.05f * (level - env);

        // LFO
        float r = rate.process();
        phase += r / (float)gam::sampleRate();
        if(phase >= 1.f) phase -= 1.f;

        float lfo = std::sin(phase * 6.2831853f);

        // Combined motion
        float motion = lfo * depth + env * envAmt;

        float cutoff = baseFreq * std::pow(2.f, motion * 3.f);
        cutoff = std::clamp(cutoff, 40.f, 0.45f * (float)gam::sampleRate());

        float in = x * im.process() + last * baseFB;

        for(auto& s : chain) {
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
    float envAmt;

    Modulator im, am;

    float phase = 0.f;
    float env   = 0.f;
    float last  = 0.f;
};
