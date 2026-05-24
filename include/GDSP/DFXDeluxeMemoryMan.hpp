#pragma once
#include <algorithm>
#include <cmath>

#include "Engine.hpp"
#include "DFXDigitalDelay.hpp"
#include "DFXBBDDelay.hpp"


// --- Memory Man mode switch ---
enum class DMMMode { CHORUS = 0, VIBRATO = 1 };

// --- Final “Deluxe Memory Man” wrapper ---
class DFXDeluxeMemoryMan : public Function
{
public:
    DFXDeluxeMemoryMan(float maxDelay = 2.0f)
    : mDly(maxDelay, 0.35f)
    {
        // sensible “DMM-ish” defaults
        setDelayKnob(0.55f);
        setFeedbackKnob(0.45f);
        setBlendKnob(0.35f);
        setRateKnob(0.35f);
        setDepthKnob(0.35f);
        setMode(DMMMode::CHORUS);

        // internal trims (feel free to expose)
        setToneTrim(0.55f);
        setDriveTrim(0.35f);
        setNoiseTrim(0.35f);
        setClockBleedTrim(0.35f);
        setLevelTrim(0.60f);
    }

    // ---- Main knobs (0..1) ----
    void setDelayKnob(float n)
    {
        mK_delay = clamp01(n);

        // DMM delay range: ~35ms .. ~550ms (feel free to widen)
        // Exponential mapping gives better low-time control.
        mDelaySec = expMap(mK_delay, 0.035f, 0.55f, 3.2f);
        mDly.setDelay(mDelaySec);
        pushModDepth();
    }

    void setFeedbackKnob(float n)
    {
        mK_fbk = clamp01(n);

        // DMM regen: gentle early, then ramps hard near self-osc
        float a = std::pow(mK_fbk, 2.2f);
        mFeedback = std::clamp(0.02f + a * 0.985f, 0.0f, 0.985f);
        mDly.setFeedback(mFeedback);
    }

    void setBlendKnob(float n)
    {
        mK_blend = clamp01(n);

        // Smoothstep gives a nice “center weight”
        float x = mK_blend;
        mBlend = x * x * (3.f - 2.f * x); // smoothstep

        applyWetDry();
    }

    void setRateKnob(float n)
    {
        mK_rate = clamp01(n);

        // DMM modulation rate feels exponential
        mRateHz = expMap(mK_rate, 0.05f, 6.0f, 3.0f);
        mDly.setLFO(mRateHz);
    }

    void setDepthKnob(float n)
    {
        mK_depth = clamp01(n);
        pushModDepth();
    }

    void setMode(DMMMode mode)
    {
        mMode = mode;
        applyWetDry();
        pushModDepth();
    }

    // ---- Trims (0..1) ----
    void setToneTrim(float n)
    {
        // Tone here is modeled as clock range shaping (brighter/darker BBD bandwidth)
        mK_tone = clamp01(n);

        // Darker -> lower max clock and lower min clock
        float minC = 5000.f + 5000.f * mK_tone;     // 5k..10k
        float maxC = 14000.f + 14000.f * mK_tone;   // 14k..28k
        mDly.setClockRange(minC, maxC);
    }

    void setDriveTrim(float n)
    {
        mK_drive = clamp01(n);
        // 0..1 -> 0.8..2.0 (more drive = more compression/grit)
        float d = 0.8f + 1.2f * mK_drive;
        mDly.setBBDDrive(d);
    }

    void setNoiseTrim(float n)
    {
        mK_noise = clamp01(n);
        // 0..1 -> ~0.0006..0.004
        float noise = 0.0006f + 0.0034f * (mK_noise * mK_noise);
        mDly.setBBDNoise(noise);
    }

    void setClockBleedTrim(float n)
    {
        mK_bleed = clamp01(n);
        float bleed = 0.00010f + 0.00060f * (mK_bleed * mK_bleed);
        mDly.setClockBleed(bleed);
    }

    void setLevelTrim(float n)
    {
        mK_level = clamp01(n);
        // -8dB .. +3dB
        float db = -8.0f + 11.0f * mK_level;
        mLevel = std::pow(10.f, db / 20.f);
    }

    // ---- DSP ----
    float process(float x) override
    {
        float y = mDly.process(x);
        return y * mLevel;
    }

private:
    static float clamp01(float x) { return std::clamp(x, 0.f, 1.f); }

    static float expMap(float n, float a, float b, float k)
    {
        n = clamp01(n);
        float e = (std::exp(k * n) - 1.f) / (std::exp(k) - 1.f);
        return a + (b - a) * e;
    }

    void applyWetDry()
    {
        // DMM chorus: normal blend
        // DMM vibrato: mostly wet (classic “pitchy” mode)
        float wet = mBlend;
        float dry = 1.0f - mBlend;

        if (mMode == DMMMode::VIBRATO)
        {
            // Bias towards wet; allow user blend to still matter.
            wet = std::clamp(0.65f + 0.35f * mBlend, 0.f, 1.f);
            dry = 1.0f - wet;
        }

        mDly.setWet(wet);
        mDly.setDry(dry);

        // keep mix at 1.0 inside DFXDigitalDelay so wet/dry controls dominate
        // (your DFXDigitalDelay uses mix to crossfade between dry and wet internally)
        mDly.setMix(1.0f);
    }

    void pushModDepth()
    {
        // DMM modulation depth is *small* because it modulates delay time.
        // Depth should scale with delay time: longer time can tolerate slightly bigger modulation.
        float baseDepth = 0.0020f;                  // chorus-ish fraction
        float vibDepth  = 0.0060f;                  // vibrato-ish fraction

        float maxD = (mMode == DMMMode::VIBRATO) ? vibDepth : baseDepth;

        // sqrt mapping gives useful low-end control
        float d = std::sqrt(mK_depth) * maxD;

        // Your DFXDigitalDelay computes: delayT = baseDelay * (1 + lfo*depth)
        // so depth is fractional.
        mDly.setDepth(d);
    }

private:
    DFXBBDDelay mDly;
    DMMMode mMode = DMMMode::CHORUS;

    // Knobs (0..1)
    float mK_delay = 0.55f;
    float mK_fbk   = 0.45f;
    float mK_blend = 0.35f;
    float mK_rate  = 0.35f;
    float mK_depth = 0.35f;

    // Trims (0..1)
    float mK_tone  = 0.55f;
    float mK_drive = 0.35f;
    float mK_noise = 0.35f;
    float mK_bleed = 0.35f;
    float mK_level = 0.60f;

    // Mapped params
    float mDelaySec = 0.35f;
    float mFeedback = 0.45f;
    float mBlend    = 0.35f;
    float mRateHz   = 0.25f;
    float mLevel    = 1.0f;
};
