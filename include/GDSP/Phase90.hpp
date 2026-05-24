#pragma once
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>

#ifndef SWIG
#include "Allpass2Block.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"
#endif

class Phase90 : public Function {
public:
    Phase90()
    {
        // Phase 90 default character
        baseMin = 350.f;
        baseMax = 1200.f;
        baseWidth = 180.f;
        baseFB = 0.48f;
        mix = 1.f;

        setRate(0.25f);

        // Fixed geometric spread
        const float spread[4] = { 0.93f, 0.98f, 1.02f, 1.07f };

        std::mt19937 rng(1337);
        std::uniform_real_distribution<float> tolF(0.99f, 1.01f);
        std::uniform_real_distribution<float> tolW(0.98f, 1.02f);

        for (int i = 0; i < 4; ++i)
        {
            float f = 700.f * spread[i] * tolF(rng);
            float w = baseWidth * tolW(rng);
            stages.emplace_back(f, w);
        }
    }

    // ---------------- Controls ----------------

    void setRate(float hz) { lfoFreq.set(hz); }

    void setDepth(float d) { depth = std::clamp(d, 0.f, 1.f); }

    void setFeedback(float fb) { baseFB = std::clamp(fb, 0.f, 0.95f); }

    void setMix(float m) { mix = std::clamp(m, 0.f, 1.f); }

    void reset()
    {
        for (auto& s : stages) s.reset();
        last = 0.f;
        lfoPhase = 0.f;
    }

    float process(float in) override
    {
        // ---- LFO ----
        float u = nextLFO();      // 0..1
        float mod = shapePhase90(u);

        float fc = baseMin + (baseMax - baseMin) * mod;

        // ---- Feedback loop ----
        float x = in + last * baseFB;
        x = std::tanh(x * 1.3f);   // chew

        for (auto& s : stages)
        {
            s.freq(fc);
            s.width(baseWidth);
            x = s.process(x);
        }

        last = x;

        return in * (1.f - mix) + x * mix;
    }

private:
    // Phase 90 LFO shaping
    float shapePhase90(float u)
    {
        float tri = 1.f - std::fabs(2.f*u - 1.f);
        float s   = 0.5f - 0.5f * std::cos(2.f * 3.14159265359f * u);
        float x   = 0.65f * tri + 0.35f * s;

        float amt = 0.28f;
        x = x + amt * (0.5f - x) * (1.f - 2.f * std::fabs(x - 0.5f));

        return std::clamp(x, 0.f, 1.f);
    }

    float nextLFO()
    {
        float inc = lfoFreq.process() / gam::sampleRate();
        lfoPhase += inc;
        if (lfoPhase >= 1.f) lfoPhase -= 1.f;
        return lfoPhase;
    }

private:
    std::vector<AllPass2> stages;

    float baseMin = 350.f;
    float baseMax = 1200.f;
    float baseWidth = 180.f;

    float baseFB = 0.48f;
    float mix = 1.f;

    float last = 0.f;

    ParamLinear lfoFreq { 0.25f };
    float lfoPhase = 0.f;
    float depth = 1.f;
};
