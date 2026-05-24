// ChorusModule.hpp
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include "GammaDSP.hpp"

class DFXChorusModule {
public:
    
    DFXChorusModule(float max_delay=2.0f)
    {
        sampleRate = gam::sampleRate();
        maxDelay   = max_delay;

        for (auto& d : delayL) d = Delay(sr, maxDelay, 0.015f);
        for (auto& d : delayR) d = Delay(sr, maxDelay, 0.015f);

        smoothCoeff = std::exp(-1.f / (0.02f * sampleRate));
    }

    // ===== Controls =====
    void setRate(float r)   { rate = std::max(0.f, r); }
    void setDepth(float d)  { depth = std::clamp(d, 0.f, 1.f); }
    void setMix(float m)    { mix   = std::clamp(m, 0.f, 1.f); }
    void setWidth(float w)  { width = std::clamp(w, 0.f, 1.f); }

    void process(float& L, float& R)
    {
        float dryL = L, dryR = R;

        phase += rate / sampleRate;
        if (phase > 1.f) phase -= 1.f;

        float wetL = 0.f, wetR = 0.f;

        for (int i = 0; i < voices; ++i)
        {
            float p = phase + i * phaseOffset;
            if (p > 1.f) p -= 1.f;

            float lfo = 0.5f + 0.5f * std::sin(p * twoPi);

            float mod = baseDelay + lfo * depth * modDepth;

            smoothTime[i] = smoothCoeff * smoothTime[i] + (1.f - smoothCoeff) * mod;

            delayL[i].setTime(smoothTime[i]);
            delayR[i].setTime(smoothTime[i] + width * stereoOffset);

            wetL += delayL[i].process(dryL);
            wetR += delayR[i].process(dryR);
        }

        wetL *= invVoices;
        wetR *= invVoices;

        L = dryL * (1.f - mix) + wetL * mix;
        R = dryR * (1.f - mix) + wetR * mix;
    }

private:
    static constexpr int voices = 3;
    static constexpr float maxDelay = 0.05f;
    static constexpr float baseDelay = 0.015f;
    static constexpr float modDepth  = 0.010f;
    static constexpr float stereoOffset = 0.003f;

    static constexpr float phaseOffset = 0.33f;
    static constexpr float twoPi = 6.28318530718f;

    float sampleRate = 44100.f;

    std::array<Delay, voices> delayL, delayR;
    std::array<float, voices> smoothTime{};

    float rate = 0.25f;
    float depth = 0.5f;
    float mix = 0.35f;
    float width = 0.7f;

    float phase = 0.f;
    float smoothCoeff = 0.999f;

    static constexpr float invVoices = 1.f / voices;
};
