#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include "GDSP_ModAllPass1.hpp"
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

class GroupDelayModulator : public Function
{
public:
    GroupDelayModulator(int stages = 12)
    : baseFreq(600.f),
      baseWidth(2.0f),
      baseFB(0.4f),
      baseMix(0.7f)
    {
        setRate(0.2f);
        setDepth(0.8f);
        setDispersion(0.6f);
        setIM(1.f);
        setAM(1.f);

        for(int i = 0; i < stages; ++i)
            chain.emplace_back(baseFreq);
    }

    // -------- Controls --------
    void setRate(float r)       { rate.set(std::max(0.01f, r)); }
    void setDepth(float d)      { depth = std::clamp(d, 0.f, 1.f); }
    void setDispersion(float d) { dispersion = std::clamp(d, 0.f, 1.f); }
    void setFeedback(float f)   { baseFB = std::clamp(f, -0.99f, 0.99f); }
    void setMix(float m)        { baseMix = std::clamp(m, 0.f, 1.f); }

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float x) override
    {
        // LFO
        float r = rate.process();
        phase += r / (float)gam::sampleRate();
        if(phase >= 1.f) phase -= 1.f;

        float lfo = std::sin(phase * 6.2831853f);

        // Motion amount
        float motion = lfo * depth;

        float center = baseFreq * std::pow(2.f, motion * 3.f);
        center = std::clamp(center, 40.f, 0.45f * (float)gam::sampleRate());

        float in = x * im.process() + last * baseFB;

        const int N = (int)chain.size();
        for(int i = 0; i < N; ++i)
        {
            float norm = (float)i / (float)(N - 1);
            float spread = (norm - 0.5f) * dispersion * 4.f;

            float f = center * std::pow(2.f, spread);
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
    float baseWidth;
    float baseFB;
    float baseMix;

    float dispersion;
    float depth;

    Modulator rate;
    Modulator im, am;

    float phase = 0.f;
    float last  = 0.f;
};
