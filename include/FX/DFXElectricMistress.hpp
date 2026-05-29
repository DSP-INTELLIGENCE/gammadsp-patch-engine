// ElectricMistress.hpp
#pragma once
#include <cmath>
#include <algorithm>
#include "GDSP_Delay.hpp"

// Simple helpers
static inline float clamp(float x, float a, float b) {
    return std::clamp(x, a, b);
}

// Reuse your existing blocks
// TapeSat, TapeDamp (or simple LP), etc.

class DFXElectricMistress : public Function {
public:
    explicit ElectricMistress(float sampleRate = 48000.f)
    : mSR(sampleRate),
      delay(0.01f, 0.001f)  // max 10 ms, start ~1 ms
    {
        delay.setIpolType(ALLPASS);   // critical for "liquid" sound

        baseDelay = 0.0008f;
        depth     = 0.0025f;
        rate      = 0.15f;
        feedback  = 0.75f;
        mix       = 0.5f;

        lp.set(8000.f, mSR);
        sat.setDrive(1.8f);
    }

    void setRate(float r)     { rate = clamp(r, 0.01f, 2.0f); }
    void setDepth(float d)    { depth = clamp(d, 0.f, 0.005f); }
    void setFeedback(float f) { feedback = clamp(f, 0.f, 0.95f); }
    void setMix(float m)      { mix = clamp(m, 0.f, 1.f); }

    float process(float x) override
    {
        // LFO
        phase += rate * twoPi / mSR;
        if (phase > twoPi) phase -= twoPi;

        float lfo = std::sin(phase);

        // Delay sweep
        float d = baseDelay + depth * lfo;
        d = std::max(d, 0.0001f);
        delay.setDelay(d);

        // Core loop
        float wet = delay.read();

        // Coloration
        wet = lp.process(wet);
        wet = sat.process(wet);

        // Feedback
        delay.write(x + wet * feedback);

        // Output mix
        return x * (1.f - mix) + wet * mix;
    }

private:
    float mSR;

    Delay delay;
    TapeSat  sat;
    TapeDamp lp;

    float baseDelay, depth, rate, feedback, mix;
    float phase = 0.f;

    const float twoPi = 6.28318530718f;
};
