// GDSP_ModalReverb.hpp
#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstddef>

#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

#include "PhaseDiffuser.hpp"
#include "OnePole.hpp"
#include "ModDelay.hpp"
#include "GDSP_ModBiquad.hpp"   // your ModBiquad wrapper (BAND_PASS)

// ------------------------------------------------------------
// ModalReverb (mono)
// - Predelay (ModDelay used as simple delay)
// - Phase diffusion (smears attack / kills echoes)
// - 2–4 parallel ModalResonators (modal bodies) summed
// - Optional late diffusion
// - Physical loss shaping (HP + LP)
// - Stable wet normalization + wet/dry mix
//
// This is a "physical plate / chamber" style reverb:
// dense resonant tail without sounding like echoes.
// ------------------------------------------------------------

class ModalResonator : public Function
{
public:
    struct Mode {
        float freq  = 1000.f;   // Hz
        float decay = 0.995f;   // internal coefficient semantics depend on ModBiquad::setCoefFromFreq
        float gain  = 1.f;
        bool  dirty = true;
    };

    explicit ModalResonator(int modes = 12)
    : baseMix(1.0f) // resonator outputs "wet" by default
    {
        setIM(1.f);
        setAM(1.f);
        resize(modes);
    }

    void resize(int modes)
    {
        modes = std::max(1, modes);
        filters.assign((size_t)modes, ModBiquad(1000.f, 0.9f, gam::BAND_PASS));
        params.assign((size_t)modes, Mode{});
        for (int i = 0; i < modes; ++i)
        {
            params[(size_t)i].freq  = 180.f + 65.f * (float)i;
            params[(size_t)i].decay = 0.996f;
            params[(size_t)i].gain  = 1.f / (1.f + 0.25f * (float)i);
            params[(size_t)i].dirty = true;
        }
        updateNorm();
    }

    int numModes() const { return (int)filters.size(); }

    void setMode(int i, float freqHz, float decayCoeff, float gain)
    {
        if (i < 0 || i >= (int)filters.size()) return;
        auto& p = params[(size_t)i];
        p.freq  = freqHz;
        p.decay = decayCoeff;
        p.gain  = gain;
        p.dirty = true;
        updateNorm();
    }

    // Musical convenience: approximate T60 -> decay coefficient
    // NOTE: This is a generic mapping; tune to your ModBiquad's exact decay meaning if needed.
    void setModeT60(int i, float t60Seconds, float freqHz, float gain)
    {
        t60Seconds = std::max(0.01f, t60Seconds);
        const float sr = (float)gam::sampleRate();

        // "per-sample amplitude multiplier" for -60 dB over t60:
        // a^(t60*sr) = 10^(-60/20) = 0.001  =>  a = 0.001^(1/(t60*sr))
        const float a = std::pow(0.001f, 1.f / (t60Seconds * sr));

        setMode(i, freqHz, std::clamp(a, 0.0f, 0.9999f), gain);
    }

    // Exciter injection: makes it behave like an object that rings after input stops
    void excite(float impulse)
    {
        exciteEnergy += impulse;
    }

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float x) override
    {
        float in = x * im.process();

        // impulse-style excitation (one-sample injection)
        in += exciteEnergy;
        exciteEnergy = 0.f;

        float y = 0.f;

        const float sr = (float)gam::sampleRate();
        const float ny = 0.49f * sr;

        for (size_t i = 0; i < filters.size(); ++i)
        {
            auto& f = filters[i];
            auto& p = params[i];

            if (p.dirty)
            {
                const float fc = std::clamp(p.freq, 5.0f, ny);
                const float dc = std::clamp(p.decay, 0.0f, 0.9999f);
                f.setType(gam::BAND_PASS);
                f.setCoefFromFreq(fc, dc);
                p.dirty = false;
            }

            y += f.process(in) * p.gain;
        }

        y *= norm;
        return (y * baseMix) * am.process();
    }

private:
    void updateNorm()
    {
        float sum = 0.f;
        for (auto& p : params) sum += std::fabs(p.gain);
        norm = (sum > 1e-6f) ? (1.0f / sum) : 1.0f;
    }

private:
    std::vector<ModBiquad> filters;
    std::vector<Mode> params;

    float baseMix = 1.0f;
    float norm = 1.0f;

    float exciteEnergy = 0.f;
    Modulator im, am;
};

class ModalReverb : public Function
{
public:
    // bodies = number of parallel modal banks (2..4 is typical)
    // modesPerBody = modes in each bank (8..24 typical)
    ModalReverb(int bodies = 3, int modesPerBody = 14)
    : predelay(0.20f, 0.015f),
      preDiff(10),
      postDiff(8),
      air(9000.f, gam::LOW_PASS),
      bodyHP(90.f, gam::HIGH_PASS)
    {
        bodies = std::clamp(bodies, 1, 4);
        banks.resize((size_t)bodies);

        for (int b = 0; b < bodies; ++b)
            banks[(size_t)b] = ModalResonator(modesPerBody);

        // predelay as plain delay
        predelay.setDM(0.f);
        predelay.setFBM(0.f);
        predelay.setMIXM(1.f); // wet-only from predelay
        predelay.setIM(1.f);
        predelay.setFM(1.f);
        predelay.setAM(1.f);

        // diffusion defaults
        preDiff.setMix(1.f);
        postDiff.setMix(1.f);

        setMix(0.35f);
        setPreDelayMs(18.f);

        setSize(0.75f);
        setDecay(0.85f);
        setBrightness(0.60f);
        setDiffusion(0.85f);

        setMaterial(0.75f);   // how inharmonic / clustered the modes are
        setExciteAmount(0.0f);

        reset();
    }

    // ---------------- Controls ----------------
    void setMix(float v) { mix = std::clamp(v, 0.f, 1.f); }

    void setPreDelayMs(float ms)
    {
        ms = std::clamp(ms, 0.f, 200.f);
        predelay.setDelay(ms * 0.001f);
    }

    // Size mostly affects modal distribution scale
    void setSize(float v)
    {
        size = std::clamp(v, 0.f, 1.f);
        retune();
    }

    // Decay controls per-mode T60 scaling (longer at low frequencies, shorter at highs)
    void setDecay(float v)
    {
        decay = std::clamp(v, 0.f, 1.f);
        retune();
    }

    // Brightness controls loop air loss (LP) and low cut (HP)
    void setBrightness(float v)
    {
        bright = std::clamp(v, 0.f, 1.f);
        // darker -> lower LP cutoff; brighter -> higher cutoff
        air.setFreq(2500.f + bright * 11000.f);
        // darker -> a bit more HP to reduce boom in long tails
        bodyHP.setFreq(50.f + (1.f - bright) * 140.f);
    }

    void setDiffusion(float v)
    {
        diffusion = std::clamp(v, 0.f, 1.f);
        preDiff.setDepth(0.25f + 0.75f * diffusion);
        postDiff.setDepth(0.15f + 0.85f * diffusion);
    }

    // Material: 0=more harmonic/wood-ish, 1=more inharmonic/plate-ish
    void setMaterial(float v)
    {
        material = std::clamp(v, 0.f, 1.f);
        retune();
    }

    // If you want "struck object" behavior from input transients:
    // This injects a little impulse energy into the modal banks each sample based on |input|.
    void setExciteAmount(float v) { exciteAmt = std::clamp(v, 0.f, 1.f); }

    void reset()
    {
        // (ModalResonator doesn’t expose a full reset; easiest is to re-tune and rely on filter state.)
        // If your ModBiquad has reset(), call it inside ModalResonator and expose a reset() there.
        // We'll at least clear the diffusers/filters.
        // PhaseDiffuser has no reset in your snippet; if it does, call it.
        air.reset(0.f);
        bodyHP.reset(0.f);
    }

    // ---------------- Audio ----------------
    float process(float x) override
    {
        // predelay
        float in = predelay.process(x);

        // pre-diffuse to remove direct comb structure
        in = preDiff.process(in);

        // optional transient-to-impulse excitation
        if (exciteAmt > 0.f)
        {
            float e = std::fabs(in) * (0.25f * exciteAmt);
            for (auto& b : banks) b.excite(e);
        }

        // sum parallel modal banks
        float wet = 0.f;
        for (auto& b : banks) wet += b.process(in);

        wet *= (banks.empty() ? 1.f : (1.f / (float)banks.size()));

        // post diffusion = “space” polish
        wet = postDiff.process(wet);

        // physical loss shaping
        wet = bodyHP.process(wet);
        wet = air.process(wet);

        // mix
        return x * (1.f - mix) + wet * mix;
    }

private:
    void retune()
    {
        // Generate mode sets: clustered + inharmonicity controlled by material.
        // We intentionally create slightly different banks to avoid ringing.
        const float sr = (float)gam::sampleRate();
        const float ny = 0.49f * sr;

        // Base “plate size” scaling
        const float base = 120.f + size * 520.f; // Hz reference
        const float stretch = 1.0f + material * 0.35f; // inharmonic stretch

        // Global decay mapping (T60-ish)
        const float t60Low  = 0.25f + decay * 5.0f; // seconds
        const float t60High = 0.10f + decay * 2.0f; // seconds

        for (size_t b = 0; b < banks.size(); ++b)
        {
            auto& bank = banks[b];
            const int M = bank.numModes();

            // small per-bank detune/variation
            const float bankSkew = 1.0f + 0.015f * (float)b;
            const float bankShift = 1.0f + 0.03f * (float)b * (material - 0.5f);

            for (int i = 0; i < M; ++i)
            {
                float k = (float)(i + 1);

                // mode frequency model:
                // harmonic-ish at low material, increasingly stretched at high material
                float f = base * bankSkew * std::pow(k, stretch) * bankShift;

                // add gentle clustering (plate-like groups)
                float cluster = 1.0f + 0.06f * material * std::sin(0.85f * k + 1.7f * (float)b);
                f *= cluster;

                f = std::clamp(f, 25.f, ny);

                // higher modes decay faster
                float t60 = t60Low + (t60High - t60Low) * std::clamp((f - 200.f) / 6000.f, 0.f, 1.f);

                // gains roll off with mode index and frequency
                float g = 1.f / (1.f + 0.22f * k);
                g *= 1.f / (1.f + 0.00015f * f);

                bank.setModeT60(i, t60, f, g);
            }
        }
    }

private:
    // components
    ModDelay predelay;
    PhaseDiffuser preDiff;
    PhaseDiffuser postDiff;

    OnePole air;     // LP
    OnePole bodyHP;  // HP

    std::vector<ModalResonator> banks;

    // params
    float mix = 0.35f;
    float size = 0.75f;
    float decay = 0.85f;
    float bright = 0.60f;
    float diffusion = 0.85f;
    float material = 0.75f;
    float exciteAmt = 0.0f;
};
