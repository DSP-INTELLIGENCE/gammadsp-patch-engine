// HaasWidener.hpp
#pragma once
#include <algorithm>
#include <cmath>
#include "GammaDSP.hpp"

// Mono-safer Haas-style widener:
// - symmetric +/- delay (reduces combing vs one-sided)
// - time smoothing (no clicks/zipper)
// - optional mono-protection by blending in a little M component
class HaasWidener {
public:
    void prepare(float sr)
    {
        sampleRate = sr;

        dL = Delay(sr, maxDelaySeconds, 0.f);
        dR = Delay(sr, maxDelaySeconds, 0.f);

        // ~30 ms smoothing for delay-time changes
        smoothCoeff = std::exp(-1.f / (0.03f * sampleRate));
        smoothT = 0.f;

        setWidth(0.f);
        setMonoProtect(0.25f);
    }

    // 0..1 (0 = off, 1 = maxDelayMs)
    void setWidth(float w)
    {
        width = std::clamp(w, 0.f, 1.f);
        targetT = (maxDelayMs * 0.001f) * width; // seconds
    }

    // 0..1 (0 = no protection, 1 = strongest protection)
    // This blends a bit of Mid into each side after widening.
    void setMonoProtect(float p)
    {
        monoProtect = std::clamp(p, 0.f, 1.f);
    }

    void reset()
    {
        smoothT = 0.f;
        // If your Delay has reset/clear, call them here.
        // dL.reset(); dR.reset();
    }

    void process(float& L, float& R)
    {
        // smooth the delay time (prevents clicks)
        smoothT = smoothCoeff * smoothT + (1.f - smoothCoeff) * targetT;

        // symmetric +/- delay (better mono behavior than one-sided Haas)
        float half = 0.5f * smoothT;

        dL.setTime(std::max(0.f, half));  // delay both sides equally in magnitude
        dR.setTime(std::max(0.f, half));  // we'll swap polarity via M/S below

        // Convert to M/S
        float M = 0.5f * (L + R);
        float S = 0.5f * (L - R);

        // Haas on Side only: delay the side component (creates width without shifting center)
        float Sd = dL.process(S);

        // Reconstruct widened stereo
        float outL = M + Sd;
        float outR = M - Sd;

        // Mono protection: blend toward pure Mid a bit (reduces combing on collapse)
        // monoProtect=0 -> no change, monoProtect=1 -> stronger center dominance
        float mp = monoProtect * width; // only protect when widening
        outL = outL * (1.f - mp) + M * mp;
        outR = outR * (1.f - mp) + M * mp;

        // Small gain trim to keep loudness steadier as width increases
        float trim = 1.f / (1.f + 0.5f * width);
        L = outL * trim;
        R = outR * trim;
    }

private:
    float sampleRate = 44100.f;

    static constexpr float maxDelayMs = 12.f;          // keep musical; >15ms starts to echo
    static constexpr float maxDelaySeconds = 0.05f;    // delay buffer size

    Delay dL{44100.f, maxDelaySeconds, 0.f};
    Delay dR{44100.f, maxDelaySeconds, 0.f};

    float width = 0.f;
    float monoProtect = 0.25f;

    float targetT = 0.f;   // seconds
    float smoothT = 0.f;   // seconds
    float smoothCoeff = 0.999f;
};

// Your MS widener is fine; keep it (mono-safe compared to Haas).
inline void msWidth(float& L, float& R, float width /*0..2*/)
{
    float M = 0.5f * (L + R);
    float S = 0.5f * (L - R);
    S *= width;
    L = M + S;
    R = M - S;
}
