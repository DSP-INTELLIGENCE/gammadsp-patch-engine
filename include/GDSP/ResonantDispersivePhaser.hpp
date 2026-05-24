#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include "GDSP_ModAllPass2.hpp"
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

class ResonantDispersivePhaser : public Function
{
public:
    ResonantDispersivePhaser(int stages = 8)
    : baseFreq(350.f),
      baseWidth(160.f),
      baseFB(0.6f),
      baseMix(0.7f),
      baseRes(0.5f)
    {
        setRate(0.22f);
        setDepth(0.85f);
        setEnvAmount(0.25f);
        setDispersion(0.7f);
        setIM(1.f);
        setAM(1.f);

        for(int i = 0; i < stages; ++i)
            chain.emplace_back(baseFreq, baseWidth);
    }

    // ---------- Musical Controls ----------
    void setRate(float r)      { rate.set(std::max(0.01f, r)); }
    void setDepth(float d)     { depth = std::clamp(d, 0.f, 1.f); }
    void setFeedback(float f)  { baseFB = std::clamp(f, -0.99f, 0.99f); }
    void setResonance(float r) { baseRes = std::clamp(r, 0.f, 1.f); }
    void setDispersion(float d){ disp = std::clamp(d, 0.f, 1.f); }
    void setMix(float m)       { baseMix = std::clamp(m, 0.f, 1.f); }
    void setEnvAmount(float e) { envAmt = std::clamp(e, 0.f, 1.f); }

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float x) override
    {
        const float sr = (float)gam::sampleRate();

        // Envelope follower
        float level = std::fabs(x);
        env += 0.03f * (level - env);

        // LFO
        float lfo = rate.process();

        // Combined motion
        float motion = lfo * depth + env * envAmt;

        float cutoff = baseFreq * std::pow(2.f, motion * 3.f);
        cutoff = std::clamp(cutoff, 40.f, 0.45f * sr);

        float width = baseWidth * (1.f + disp * (0.5f + 0.5f * motion));
        width = std::clamp(width, 1.f, 8000.f);

        // Feedback + resonance injection
        float in = x * im.process() + last * baseFB + resStore * baseRes;

        for(auto& s : chain)
        {
            s.setCutoff(cutoff);
            s.setWidth(width);
            in = s.process(in);
        }

        // Extract resonance from phase motion
        float delta = in - last;
        resStore = std::tanh(delta * 2.f);

        float out = x * (1.f - baseMix) + in * baseMix;
        out *= am.process();

        last = in;
        return out;
    }

private:
    std::vector<ModAllPass2> chain;

    float baseFreq;
    float baseWidth;
    float baseFB;
    float baseMix;
    float baseRes;

    float disp = 0.7f;
    float depth = 0.85f;
    float envAmt = 0.25f;

    Modulator rate;
    Modulator im, am;

    float env = 0.f;
    float last = 0.f;
    float resStore = 0.f;
};
