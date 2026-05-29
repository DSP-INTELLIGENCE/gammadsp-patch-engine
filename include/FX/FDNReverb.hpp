// GDSP_FDNReverb.hpp
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

#include "ModDelay.hpp"
#include "OnePole.hpp"
#include "PhaseDiffuser.hpp"

// ------------------------------------------------------------
// 4-line Feedback Delay Network (FDN) Reverb (mono)
// - Pre/Post phase diffusion (all-pass scatter)
// - 4 mod-delay lines (very light modulation to avoid ringing)
// - Hadamard feedback matrix (energy-preserving)
// - Per-line damping (LP) + rumble control (HP)
// - Stable decay control (feedback gain applied after matrix)
// ------------------------------------------------------------
class FDNReverb : public Function
{
public:
    static constexpr int N = 4;

    FDNReverb()
    : preDiff(6),
      postDiff(6)
    {
        // Delay lines: max 200ms, init will be set in setSize()
        for (int i = 0; i < N; ++i)
        {
            lines[i] = ModDelay(0.20f, 0.03f);
            lines[i].setIpolType(gam::ipl::LINEAR);
            lines[i].setDM(0.f);
            lines[i].setFBM(0.f);      // we do FDN feedback externally
            lines[i].setMIXM(1.f);     // treat each line as wet-only tap
            lines[i].setIM(1.f);
            lines[i].setFM(1.f);
            lines[i].setAM(1.f);

            lp[i] = OnePole(8000.f, gam::LOW_PASS);
            hp[i] = OnePole(80.f,   gam::HIGH_PASS);
        }

        setMix(0.35f);
        setSize(0.75f);
        setDecay(0.85f);
        setDamping(0.60f);
        setHighpass(80.f);
        setDiffusion(0.70f);
        setMod(0.15f, 0.08f); // (depth, rate) gentle by default

        reset();
    }

    // ---------------- Controls ----------------
    void setMix(float v)        { mix = std::clamp(v, 0.f, 1.f); }

    // Controls the base delay lengths (space size / modal density)
    void setSize(float v)
    {
        size = std::clamp(v, 0.f, 1.f);

        // base times (seconds) chosen to be mutually incommensurate
        // scale from ~18–90 ms (small room -> plate-ish -> hall-ish)
        const float baseScale = 0.018f + size * 0.072f;

        // ratios spread modal distribution
        const float t0 = baseScale * 1.00f;
        const float t1 = baseScale * 1.27f;
        const float t2 = baseScale * 1.58f;
        const float t3 = baseScale * 1.93f;

        lines[0].setDelay(clampDelay(t0));
        lines[1].setDelay(clampDelay(t1));
        lines[2].setDelay(clampDelay(t2));
        lines[3].setDelay(clampDelay(t3));
    }

    // Overall decay (feedback gain). Keep < 1 for stability.
    void setDecay(float v)      { decay = std::clamp(v, 0.f, 0.99f); }

    // Damping: 0..1 maps to lowpass cutoff (2k..12k)
    void setDamping(float v)
    {
        damping = std::clamp(v, 0.f, 1.f);
        const float hz = 2000.f + damping * 10000.f;
        for (int i = 0; i < N; ++i) lp[i].setFreq(hz);
    }

    // Highpass to prevent low buildup
    void setHighpass(float hz)
    {
        hpHz = hz;
        for (int i = 0; i < N; ++i) hp[i].setFreq(hz);
    }

    // Phase diffusion amount: 0..1
    void setDiffusion(float v)
    {
        diffusion = std::clamp(v, 0.f, 1.f);
        preDiff.setDepth(0.2f + 0.8f * diffusion);
        preDiff.setMix(1.f);
        postDiff.setDepth(0.2f + 0.8f * diffusion);
        postDiff.setMix(1.f);
    }

    // Very gentle modulation to break static modes (avoid chorus wobble)
    // depth ~ 0..1 (scaled internally), rate in Hz
    void setMod(float depth01, float rateHz)
    {
        modDepth = std::clamp(depth01, 0.f, 1.f);
        modRate  = std::max(0.01f, rateHz);
    }

    void reset()
    {
        phase = 0.f;
        for (int i = 0; i < N; ++i)
        {
            lp[i].reset(0.f);
            hp[i].reset(0.f);
            // ModDelay/Delay internal clear not shown in your snippet;
            // if your Delay has clear(), call it there. Otherwise, set feedback to zero.
            state[i] = 0.f;
        }
    }

    // ---------------- Audio ----------------
    float process(float x) override
    {
        const float sr = (float)gam::sampleRate();

        // Pre diffusion (kills direct combiness)
        float in = preDiff.process(x);

        // LFO for subtle delay modulation (shared)
        phase += modRate / sr;
        if (phase >= 1.f) phase -= 1.f;
        const float s = std::sin(phase * 6.2831853f);

        // Input injection to each line (simple equal feed)
        const float inj = in * 0.25f;

        // Read line outputs (wet taps). Each ModDelay returns wet because MIXM=1.
        float y[N];
        for (int i = 0; i < N; ++i)
        {
            // tiny delay-time modulation around base delay
            // dm in ModDelay is fractional multiplier of base delay: delayT = base + base*dm
            // Keep this very small to avoid pitch wobble.
            const float dm = s * (0.0015f + 0.006f * modDepth) * (i + 1);
            lines[i].setDM(dm);

            // drive the line with current injection + feedback state
            y[i] = lines[i].process(inj + state[i]);
        }

        // Hadamard (4x4) feedback mix (energy-preserving when scaled by 0.5)
        // [ 1  1  1  1]
        // [ 1 -1  1 -1]
        // [ 1  1 -1 -1]
        // [ 1 -1 -1  1]
        float m0 = ( y[0] + y[1] + y[2] + y[3]) * 0.5f;
        float m1 = ( y[0] - y[1] + y[2] - y[3]) * 0.5f;
        float m2 = ( y[0] + y[1] - y[2] - y[3]) * 0.5f;
        float m3 = ( y[0] - y[1] - y[2] + y[3]) * 0.5f;

        // Apply per-line loss + global decay, then store as next feedback state
        state[0] = loss(m0, 0) * decay;
        state[1] = loss(m1, 1) * decay;
        state[2] = loss(m2, 2) * decay;
        state[3] = loss(m3, 3) * decay;

        // Wet output (sum taps) + post diffusion (final smoothing)
        float wet = 0.25f * (y[0] + y[1] + y[2] + y[3]);
        wet = postDiff.process(wet);

        return x * (1.f - mix) + wet * mix;
    }

private:
    static inline float clampDelay(float t)
    {
        // ModDelay uses seconds; keep inside maxDelay configured (0.20s)
        return std::clamp(t, 0.0005f, 0.20f);
    }

    inline float loss(float x, int i)
    {
        // rumble control first, then air loss
        x = hp[i].process(x);
        x = lp[i].process(x);
        return x;
    }

private:
    // diffusion
    PhaseDiffuser preDiff;
    PhaseDiffuser postDiff;

    // FDN lines + per-line loss
    std::array<ModDelay, N> lines;
    std::array<OnePole,  N> lp;
    std::array<OnePole,  N> hp;

    // feedback state (per line)
    float state[N] = {0.f, 0.f, 0.f, 0.f};

    // params
    float mix = 0.35f;
    float size = 0.75f;
    float decay = 0.85f;
    float damping = 0.60f;
    float diffusion = 0.70f;
    float hpHz = 80.f;

    // modulation
    float modDepth = 0.15f;
    float modRate  = 0.08f;
    float phase = 0.f;
};
