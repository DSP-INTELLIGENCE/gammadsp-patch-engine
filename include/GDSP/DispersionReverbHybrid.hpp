#pragma once
#include <array>
#include <algorithm>
#include <cmath>

#include "Engine.hpp"
#include "Parameters.hpp"

#include "DispersivePhaser.hpp"      // from earlier
#include "DispersionNetwork.hpp"     // your engine-native version (ModAllPass2 + OnePole loss)
#include "PhaseDiffuser.hpp"
#include "ModDelay.hpp"
#include "OnePole.hpp"

// ------------------------------------------------------------
// DispersionReverbHybrid (mono)
// DispersivePhaser  ->  4-line mini-FDN  ->  Dispersion shimmer-in-loop  ->  output
//
// Goals:
// - "liquid metal" early smear (group-delay sculptor front-end)
// - dense, echo-free late tail (FDN + diffusion)
// - living shimmer without pitch wobble (DispersionNetwork inside feedback)
// - hard stability: loss filters + tanh limiter in the loop
// ------------------------------------------------------------
class DispersionReverbHybrid : public Function
{
public:
    static constexpr int N = 4;

    DispersionReverbHybrid()
    : preDiff(8),
      postDiff(8),
      shimmer(2, 6),                 // layers, stages (cheap shimmer fabric)
      airLP(9000.f, gam::LOW_PASS),
      bodyHP(90.f,  gam::HIGH_PASS)
    {
        // --- delay lines (max 250ms) ---
        for (int i = 0; i < N; ++i)
        {
            lines[i] = ModDelay(0.25f, 0.03f);
            lines[i].setIpolType(gam::ipl::LINEAR);

            // treat each line as "wet tap"
            lines[i].setDM(0.f);
            lines[i].setFBM(0.f);     // external feedback only
            lines[i].setMIXM(1.f);
            lines[i].setIM(1.f);
            lines[i].setFM(1.f);
            lines[i].setAM(1.f);

            lp[i] = OnePole(8000.f, gam::LOW_PASS);
            hp[i] = OnePole(80.f,   gam::HIGH_PASS);
        }

        // diffusion blocks
        preDiff.setMix(1.f);
        postDiff.setMix(1.f);
        setDiffusion(0.85f);

        // shimmer network defaults (stable)
        shimmer.setMix(1.f);
        shimmer.setFeedback(0.0f);
        shimmer.setDrive(1.0f);
        shimmer.setDepth(0.75f);
        shimmer.setRate(0.05f);
        shimmer.setDispersion(0.80f);

        // front-end dispersive phaser defaults
        front.setMix(1.0f);
        front.setFeedback(0.25f);
        front.setDispersion(0.75f);
        front.setDepth(0.80f);
        front.setRate(0.06f);

        setMix(0.40f);
        setSize(0.80f);
        setDecay(0.90f);
        setDamping(0.65f);
        setHighpass(80.f);

        setShimmer(0.70f);
        setMotion(0.55f);
        setDrive(0.20f);

        reset();
    }

    // ---------------- Controls ----------------
    void setMix(float v) { mix = std::clamp(v, 0.f, 1.f); }

    // Size sets base delay times (density / room scale)
    void setSize(float v)
    {
        size = std::clamp(v, 0.f, 1.f);

        // ~15ms..115ms scale, spread by incommensurate ratios
        const float base = 0.015f + size * 0.100f;
        const float t0 = base * 1.00f;
        const float t1 = base * 1.31f;
        const float t2 = base * 1.73f;
        const float t3 = base * 2.17f;

        lines[0].setDelay(clampDelay(t0));
        lines[1].setDelay(clampDelay(t1));
        lines[2].setDelay(clampDelay(t2));
        lines[3].setDelay(clampDelay(t3));
    }

    // Decay = global loop feedback gain (stable < 1)
    void setDecay(float v) { decay = std::clamp(v, 0.f, 0.99f); }

    // Damping maps to loop lowpass cutoff
    void setDamping(float v)
    {
        damping = std::clamp(v, 0.f, 1.f);
        const float hz = 2000.f + damping * 10000.f; // 2k..12k
        for (int i = 0; i < N; ++i) lp[i].setFreq(hz);
        airLP.setFreq(2500.f + damping * 11000.f);
    }

    void setHighpass(float hz)
    {
        hpHz = hz;
        for (int i = 0; i < N; ++i) hp[i].setFreq(hz);
        bodyHP.setFreq(hz);
    }

    // Diffusion controls pre/post PhaseDiffuser depth
    void setDiffusion(float v)
    {
        diffusion = std::clamp(v, 0.f, 1.f);
        preDiff.setDepth(0.25f + 0.75f * diffusion);
        postDiff.setDepth(0.15f + 0.85f * diffusion);
    }

    // Shimmer = how much dispersive fabric is injected into the feedback loop
    void setShimmer(float v) { shimmerAmt = std::clamp(v, 0.f, 1.f); }

    // Motion drives BOTH the front dispersive phaser + shimmer fabric motion
    void setMotion(float v)
    {
        motion = std::clamp(v, 0.f, 1.f);

        // front: slow and wide
        front.setDepth(0.25f + 0.75f * motion);
        front.setRate(0.02f + 0.10f * motion);

        // shimmer: slightly faster (micro-motion)
        shimmer.setDepth(0.30f + 0.70f * motion);
        shimmer.setRate(0.03f + 0.14f * motion);
    }

    // Drive = loop limiter strength (keeps huge decays musical)
    void setDrive(float v) { drive = std::clamp(v, 0.f, 1.f); }

    // Optional direct front-end controls if you want them exposed
    void setFrontMix(float v) { front.setMix(std::clamp(v, 0.f, 1.f)); }
    void setFrontFeedback(float v) { front.setFeedback(v); }
    void setFrontDispersion(float v) { front.setDispersion(v); }

    void reset()
    {
        phase = 0.f;
        for (int i = 0; i < N; ++i) state[i] = 0.f;

        airLP.reset(0.f);
        bodyHP.reset(0.f);
        // If your Delay/ModDelay exposes clear(), call it here.
    }

    // ---------------- Audio ----------------
    float process(float x) override
    {
        const float sr = (float)gam::sampleRate();

        // 1) Front-end group-delay smear / liquid metal motion
        float in = front.process(x);

        // 2) Pre diffusion to kill obvious combs
        in = preDiff.process(in);

        // 3) Shared micro-mod for delay-time instability (tiny to avoid pitch wobble)
        phase += (0.05f + 0.10f * motion) / sr;
        if (phase >= 1.f) phase -= 1.f;
        const float s = std::sin(phase * 6.2831853f);

        // 4) Inject into lines and read taps
        const float inj = 0.25f * in;

        float y[N];
        for (int i = 0; i < N; ++i)
        {
            // tiny dm around base delay
            const float dm = s * (0.0012f + 0.0045f * motion) * (float)(i + 1);
            lines[i].setDM(dm);

            // line input = injection + feedback state
            y[i] = lines[i].process(inj + state[i]);
        }

        // 5) Hadamard 4x4 feedback mix (energy-preserving with *0.5)
        float m0 = ( y[0] + y[1] + y[2] + y[3]) * 0.5f;
        float m1 = ( y[0] - y[1] + y[2] - y[3]) * 0.5f;
        float m2 = ( y[0] + y[1] - y[2] - y[3]) * 0.5f;
        float m3 = ( y[0] - y[1] - y[2] + y[3]) * 0.5f;

        // 6) Loop loss + shimmer-in-loop + limiter + decay
        state[0] = loopProcess(m0, 0, sr) * decay;
        state[1] = loopProcess(m1, 1, sr) * decay;
        state[2] = loopProcess(m2, 2, sr) * decay;
        state[3] = loopProcess(m3, 3, sr) * decay;

        // 7) Wet out + post diffusion + physical loss polish
        float wet = 0.25f * (y[0] + y[1] + y[2] + y[3]);
        wet = postDiff.process(wet);
        wet = bodyHP.process(wet);
        wet = airLP.process(wet);

        return x * (1.f - mix) + wet * mix;
    }

private:
    static inline float clampDelay(float t)
    {
        // seconds
        return std::clamp(t, 0.0005f, 0.25f);
    }

    inline float loopProcess(float v, int i, float /*sr*/)
    {
        // rumble control then damping
        v = hp[i].process(v);
        v = lp[i].process(v);

        // shimmer injection (dispersive fabric) — blended in feedback path
        if (shimmerAmt > 0.f)
        {
            // keep shimmer stable: feed a little of v into the fabric, mix back
            float sh = shimmer.process(v);
            v = v * (1.f - shimmerAmt) + sh * shimmerAmt;
        }

        // limiter in loop (drive -> stronger)
        if (drive > 0.f)
        {
            const float g = 1.f + 10.f * drive;
            v = std::tanh(v * g) / std::tanh(g);
        }
        return v;
    }

private:
    // front-end material smear
    DispersivePhaser front;

    // diffusion
    PhaseDiffuser preDiff;
    PhaseDiffuser postDiff;

    // shimmer fabric inside the loop
    DispersionNetwork shimmer;

    // FDN lines + per-line loss
    std::array<ModDelay, N> lines;
    std::array<OnePole,  N> lp;
    std::array<OnePole,  N> hp;

    // output polish (global)
    OnePole airLP;   // LP
    OnePole bodyHP;  // HP

    // feedback state per line
    float state[N] = {0.f, 0.f, 0.f, 0.f};

    // params
    float mix = 0.40f;
    float size = 0.80f;
    float decay = 0.90f;
    float damping = 0.65f;
    float diffusion = 0.85f;
    float hpHz = 80.f;

    float shimmerAmt = 0.70f;
    float motion = 0.55f;
    float drive = 0.20f;

    float phase = 0.f;
};
