#pragma once
#include <algorithm>
#include <cmath>
#include "TPT1Pole.hpp"
// Uses your base classes: Function, TPT1Pole (or any 1-pole), etc.

struct OpAmpModel : public Function
{
    // -------- public "circuit knobs" --------
    // Supplies (virtual rails)
    float vPlus  = 4.5f;     // +rail
    float vMinus = -4.5f;    // -rail

    // Internal open-loop gain (linear)
    float Aol = 20000.0f;    // ~86 dB (good default)

    // Gain-bandwidth product (Hz)
    float gbwHz = 1.0e6f;    // 1 MHz-ish (4558-ish ballpark)

    // Slew rate (V/s)
    float slewVps = 1.0e6f;  // 1 V/us = 1e6 V/s

    // Input bias / offset
    float inputOffsetV = 0.0f;     // DC offset at input
    float inputAsym    = 0.0f;     // -1..+1 (shifts symmetry a bit)

    // Output saturation softness (bigger = softer knee)
    float railSoftness = 2.0f;

    // Optional extra “output stage” drive
    float outDrive = 1.0f;

    // -------- feedback topology --------
    // You provide external feedback gain (classic non-inverting/inverting networks).
    // This model expects you to feed back y via "fbGain".
    float fbGain = 0.0f;     // 0 = no feedback, ~0.5..0.99 typical for strong loop

    // -------- internal filters/state --------
    // Simple internal dominant pole to enforce GBW.
    // Closed-loop bandwidth ~ gbw / closedLoopGain.
    TPT1Pole domPole;

    float y = 0.0f;          // output state (post slew & rails)
    float yPre = 0.0f;       // internal (pre-rails) state

    float sr = 48000.0f;

    // -------- setup --------
    void prepare(float sampleRate)
    {
        sr = std::max(1.0f, sampleRate);
        domPole.reset(0.0f);
        updateDominantPole();
        reset();
    }

    void reset()
    {
        y = 0.0f;
        yPre = 0.0f;
        domPole.reset(0.0f);
    }

    // Call whenever gbwHz / Aol changes
    void updateDominantPole()
    {
        // Aol(s) ~ Aol / (1 + s/wp), with GBW ≈ Aol * fp  (fp = wp/2pi)
        // so fp ≈ GBW / Aol
        float fp = std::max(1.0f, gbwHz / std::max(1.0f, Aol));
        domPole.setCut(fp, sr);
    }

    // -------- helpers --------
    inline float zap(float x) const
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    inline float softRail(float x) const
    {
        // Soft rail clamp using tanh around rails (fast + musical)
        // Map x into -1..1 region relative to rails:
        float mid = 0.5f * (vPlus + vMinus);
        float half = 0.5f * (vPlus - vMinus);
        half = std::max(1e-6f, half);

        float u = (x - mid) / half;           // -inf..inf
        float s = std::tanh(u * railSoftness);
        return mid + half * s;
    }

    inline float slewLimit(float target)
    {
        // max delta per sample = slewVps / sr
        float maxStep = slewVps / sr;
        float d = target - y;
        if (d >  maxStep) y += maxStep;
        else if (d < -maxStep) y -= maxStep;
        else y = target;
        return y;
    }

    // -------- the op-amp core --------
    // process(x) expects x = input node (non-inverting) for a simple block.
    // You can also use process(x, fb) variants later, but this keeps it easy to wire.
    float process(float x) override
    {
        // 1) input conditioning / asymmetry / tiny bias
        float xin = x + inputOffsetV;
        xin += inputAsym * 0.02f * xin * xin; // subtle asym (cheap transistor-ish hint)

        // 2) feedback subtraction (loop)
        // error = vin - fbGain * y
        float e = xin - fbGain * y;

        // 3) open-loop amplification
        float v = Aol * e;

        // 4) dominant pole (GBW behavior)
        // internal compensation => lowpass the open-loop node
        float vComp = domPole.processLP(v);

        // 5) output stage "drive" + rail softening
        float vDriven = vComp * outDrive;
        float vClipped = softRail(vDriven);

        // 6) slew rate limiting on the *output transition*
        float vSlewed = slewLimit(vClipped);

        // 7) cleanup
        y = zap(vSlewed);
        return y;
    }
};
