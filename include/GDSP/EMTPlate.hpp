// EMTPlate.hpp
#pragma once
#include <cmath>
#include <algorithm>
#include <memory>
#include <utility>

// EMTPlate for your Gamma-wrapped GDSP types:
// - Comb (allpass capable) for diffusion
// - Delay (with interpolated delay time) for the tank
// - Biquad wrappers for HF/LF shaping inside the feedback loop
//
// This version adds:
// ✅ cross-feedback matrix (biggest “EMT” upgrade)
// ✅ per-node modulation phase offsets (kills metallic ringing)
// ✅ post-diffusion (silk)
// ✅ stereo taps + width control (M/S)
// ✅ subtle “steel” saturation
// ✅ delay ipolType set to CUBIC for click-free modulation

class EMTPlate : public Function {
public:
    explicit EMTPlate(float sampleRate = 48000.f)
    : mSR(sampleRate)
    {
        // -------------------------
        // Input diffusion (fast density)
        // -------------------------
        for (int i = 0; i < 4; ++i) {
            inputAP[i] = std::make_unique<Comb>(0.05f);
            inputAP[i]->setAllPass(0.75f);
        }
        inputAP[0]->setDelay(0.007f);
        inputAP[1]->setDelay(0.011f);
        inputAP[2]->setDelay(0.015f);
        inputAP[3]->setDelay(0.019f);

        // -------------------------
        // Post diffusion (silk)
        // -------------------------
        for (int i = 0; i < 2; ++i) {
            postAP[i] = std::make_unique<Comb>(0.05f);
            postAP[i]->setAllPass(0.70f);
        }
        postAP[0]->setDelay(0.009f);
        postAP[1]->setDelay(0.013f);

        // -------------------------
        // Plate tank delays
        // -------------------------
        for (int i = 0; i < 4; ++i) {
            tankDelay[i] = std::make_unique<Delay>(0.2f, 0.08f);
            tankDelay[i]->setIpolType(gam::ipl::Type::CUBIC); // important for modulated delay
        }

        tankTimes[0] = 0.067f;
        tankTimes[1] = 0.073f;
        tankTimes[2] = 0.089f;
        tankTimes[3] = 0.101f;

        for (int i = 0; i < 4; ++i)
            tankDelay[i]->setDelay(tankTimes[i]);

        // -------------------------
        // Damping / tone shaping (in feedback path)
        // -------------------------
        // HF damping: low-pass (6–9k typical)
        hfLP = std::make_unique<Biquad>(8000.f, 0.707f, 1.0f, gam::LOW_PASS);

        // LF roll-off: high-pass (200–400 typical)
        lfHP = std::make_unique<Biquad>(300.f, 0.707f, 1.0f, gam::HIGH_PASS);

        // Defaults (good EMT-ish)
        decay    = 0.89f;
        modDepth = 0.00042f; // seconds (0.42 ms)
        modRate  = 0.18f;    // Hz
        drive    = 0.18f;    // subtle
        width    = 0.85f;
        mix      = 1.0f;
        wetGain  = 0.95f;
        dryGain  = 1.0f;
    }

    // ---- controls ----
    void setDecay(float d)        { decay = std::clamp(d, 0.1f, 0.99f); }
    void setHF(float hz)          { if (hfLP) hfLP->setFreq(std::max(5.f, hz)); }
    void setLF(float hz)          { if (lfHP) lfHP->setFreq(std::max(5.f, hz)); }
    void setModDepth(float dSec)  { modDepth = std::max(0.0f, dSec); }
    void setModRate(float rHz)    { modRate  = std::max(0.0f, rHz); }
    void setDrive(float d01)      { drive    = std::clamp(d01, 0.0f, 1.0f); }
    void setWidth(float w01)      { width    = std::clamp(w01, 0.0f, 1.0f); }
    void setMix(float m01)        { mix      = std::clamp(m01, 0.0f, 1.0f); }
    void setWetGain(float g)      { wetGain  = std::max(0.0f, g); }
    void setDryGain(float g)      { dryGain  = std::max(0.0f, g); }

    // Keep your legacy mono API
    float process(float x) override {
        StereoSample s = processStereo(x);
        return 0.5f * (s.outL + s.outR);
    }

    // Proper stereo output (recommended)
    StereoSample processStereo(float x)
    {
        // -------------------------
        // Input diffusion
        // -------------------------
        float in = x;
        for (int i = 0; i < 4; ++i)
            in = inputAP[i]->process(in);

        // -------------------------
        // Per-node modulation (phase offsets)
        // -------------------------
        phase += (modRate * twoPi) / mSR;
        if (phase >= twoPi) phase -= twoPi;

        float modSig[4];
        modSig[0] = std::sin(phase) * modDepth;
        modSig[1] = std::sin(phase + 0.5f * pi) * modDepth;
        modSig[2] = std::sin(phase + 1.0f * pi) * modDepth;
        modSig[3] = std::sin(phase + 1.5f * pi) * modDepth;

        // -------------------------
        // Read tank + damping
        // -------------------------
        float v[4];
        for (int i = 0; i < 4; ++i) {
            float d = tankDelay[i]->read();

            if (hfLP) d = hfLP->process(d); // HF damping
            if (lfHP) d = lfHP->process(d); // LF roll-off

            v[i] = d;
        }

        // -------------------------
        // Cross-feedback matrix (density engine)
        // -------------------------
        float fb[4];
        fb[0] =  0.6f*v[0] + 0.4f*v[1] - 0.4f*v[2] + 0.6f*v[3];
        fb[1] = -0.4f*v[0] + 0.6f*v[1] + 0.6f*v[2] + 0.4f*v[3];
        fb[2] =  0.4f*v[0] - 0.6f*v[1] + 0.6f*v[2] + 0.4f*v[3];
        fb[3] =  0.6f*v[0] + 0.4f*v[1] + 0.4f*v[2] - 0.6f*v[3];

        // -------------------------
        // Subtle steel saturation (VERY gentle)
        // -------------------------
        auto sat = [&](float s) {
            // drive=0 => nearly linear, drive=1 => still mild
            float k = 0.30f + 0.90f * drive;
            // normalize to keep level stable-ish
            float denom = std::tanh(k);
            return (denom > 1e-6f) ? (std::tanh(s * k) / denom) : s;
        };

        // -------------------------
        // Write back into tank (modulated delay times)
        // -------------------------
        for (int i = 0; i < 4; ++i) {
            float t = tankTimes[i] + modSig[i];
            t = std::max(0.001f, t);
            tankDelay[i]->setDelay(t);

            float f = sat(fb[i]) * decay;
            tankDelay[i]->write(in + f);
        }

        // -------------------------
        // Stereo “pickup” taps (different blends)
        // -------------------------
        float wetL = 0.55f*v[0] + 0.25f*v[1] + 0.15f*v[2] + 0.05f*v[3];
        float wetR = 0.05f*v[0] + 0.15f*v[1] + 0.25f*v[2] + 0.55f*v[3];

        // Post diffusion (silk / smear)
        wetL = postAP[0]->process(wetL);
        wetR = postAP[1]->process(wetR);

        // Width control (mid/side)
        float mid  = 0.5f * (wetL + wetR);
        float side = 0.5f * (wetL - wetR);
        side *= width;

        wetL = mid + side;
        wetR = mid - side;

        wetL *= wetGain;
        wetR *= wetGain;

        float dry = x * dryGain;

        // Wet/dry mix
        float outL = dry * (1.0f - mix) + wetL * mix;
        float outR = dry * (1.0f - mix) + wetR * mix;

        return StereoSample(outL, outR);
    }

private:
    float mSR = 48000.f;

    std::unique_ptr<Comb>  inputAP[4];
    std::unique_ptr<Comb>  postAP[2];
    std::unique_ptr<Delay> tankDelay[4];

    float tankTimes[4]{};

    // Damping in feedback path
    std::unique_ptr<Biquad> hfLP; // LOW_PASS
    std::unique_ptr<Biquad> lfHP; // HIGH_PASS

    float decay    = 0.89f;
    float modDepth = 0.00042f;
    float modRate  = 0.18f;
    float phase    = 0.0f;

    float drive   = 0.18f;
    float width   = 0.85f;
    float mix     = 1.0f;
    float wetGain = 0.95f;
    float dryGain = 1.0f;

    static constexpr float pi    = 3.14159265358979323846f;
    static constexpr float twoPi = 6.28318530717958647692f;
};
