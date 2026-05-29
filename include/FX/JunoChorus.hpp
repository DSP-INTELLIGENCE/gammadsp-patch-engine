#pragma once
#include <cmath>
#include <algorithm>

class JunoChorus
{
public:
    enum Mode {
        OFF = 0,
        I,
        II,
        I_PLUS_II
    };

    explicit JunoChorus(float sr = gam::sampleRate())
    : mSR(sr),
      delayA(0.05f, 0.018f),
      delayB(0.05f, 0.018f)
    {
        delayA.setIpolType(gam::ipl::CUBIC);
        delayB.setIpolType(gam::ipl::CUBIC);

        // Hardware-accurate defaults
        baseDelay = 0.018f;   // 18 ms
        depth     = 0.0008f;  // ~0.8 ms
        rateA     = 0.6f;     // slow
        rateB     = 6.0f;     // fast

        mode = I_PLUS_II;
    }

    // ---------------- Controls ----------------

    void setMode(Mode m) { mode = m; }
    void setDepth(float d) { depth = std::max(0.f, d); }
    void setRates(float slow, float fast)
    {
        rateA = std::max(0.f, slow);
        rateB = std::max(0.f, fast);
    }

    // ---------------- DSP ----------------

    StereoSample process(float input)
    {
        if (mode == OFF)
            return StereoSample(input, input);

        advanceLFOs();

        // --- LFOs ---
        float lfoA = std::sin(phaseA);
        float lfoB = std::sin(phaseB);

        // Stereo inversion (key to width)
        float lfoA_R = -lfoA;
        float lfoB_R = -lfoB;

        // --- Modulated delays ---
        float dA_L = baseDelay + depth * lfoA;
        float dB_L = baseDelay + depth * lfoB;
        float dA_R = baseDelay + depth * lfoA_R;
        float dB_R = baseDelay + depth * lfoB_R;

        // Left
        delayA.setDelay(std::max(0.001f, dA_L));
        delayB.setDelay(std::max(0.001f, dB_L));
        float aL = delayA.process(input);
        float bL = delayB.process(input);

        // Right
        delayA.setDelay(std::max(0.001f, dA_R));
        delayB.setDelay(std::max(0.001f, dB_R));
        float aR = delayA.process(input);
        float bR = delayB.process(input);

        // --- Mode matrix ---
        float wetL = 0.f;
        float wetR = 0.f;

        if (mode == I || mode == I_PLUS_II) {
            wetL += aL;
            wetR += aR;
        }
        if (mode == II || mode == I_PLUS_II) {
            wetL += bL;
            wetR += bR;
        }

        // --- Mix (fixed-gain like hardware) ---
        constexpr float wetGain = 0.5f;

        return StereoSample(
            input + wetL * wetGain,
            input + wetR * wetGain
        );
    }

    void run(const float* in,
             float* outL,
             float* outR,
             size_t n)
    {
        for (size_t i = 0; i < n; ++i)
        {
            StereoSample s = process(in[i]);
            outL[i] = s.outL;
            outR[i] = s.outR;
        }
    }

private:
    // ---------------- Internals ----------------

    void advanceLFOs()
    {
        constexpr float twoPi = 6.28318530718f;

        phaseA += rateA * twoPi / mSR;
        phaseB += rateB * twoPi / mSR;

        if (phaseA > twoPi) phaseA -= twoPi;
        if (phaseB > twoPi) phaseB -= twoPi;
    }

private:
    float mSR;

    Delay delayA;
    Delay delayB;

    Mode mode = I_PLUS_II;

    float baseDelay;
    float depth;
    float rateA, rateB;

    float phaseA = 0.f;
    float phaseB = 0.f;
};
