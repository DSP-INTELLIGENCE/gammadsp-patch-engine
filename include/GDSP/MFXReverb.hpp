/////////////////////////////////////////////////////////////////
// ModReverb.hpp  (skeleton that plugs into your framework)
/////////////////////////////////////////////////////////////////
#pragma once
#include <array>
#include <algorithm>
#include <cmath>

// Assumes you already have:
// - Function base class
// - MultitapDelay (Gamma wrapper in your GDSP_Delay.hpp)
// - ModDelay / Delay core
// - ModDiffuser (your cascade of short ModDelay stages)
// - OnePole filter (as used in SlapBack/DubDelay/Doubler)
// - Modulator (your modulation router)
// - softClip(), randf(), gam::sampleRate()

class ModReverb : public Function
{
public:
    // 4-line FDN is a sweet spot for CPU/quality. 8 is nicer later.
    static constexpr int FDN = 4;

    ModReverb()
    : early(0.20f /*maxDelaySeconds*/, 8 /*taps*/, 0.012f /*init*/)
    {
        // --- Defaults (musical, stable) ---
        setMix(0.35f);
        setPreDelay(0.015f);
        setSize(0.65f);
        setDecay(0.65f);
        setDamping(0.55f);
        setTone(0.6f);
        setModRate(0.10f);
        setModDepth(0.0012f);
        setWidth(1.0f);
        setDrive(0.05f);

        // Early reflections: safe baseline pattern (room-ish)
        // You can replace this per preset (plate/hall/room).
        setEarlyPatternRoom();

        // Diffusers
        // (Assumes your ModDiffuser ctor sets good defaults; we can add setters later.)
        // Primary diffuser should be denser than secondary in many designs.
        // Keep as-is in skeleton.

        // FDN lines: distinct base times (avoid common factors)
        const float baseMs[FDN] = { 29.7f, 37.1f, 41.1f, 53.3f };
        for (int i = 0; i < FDN; ++i)
        {
            fdn[i].setMM(1.0f);   // fully wet internally
            fdn[i].setFM(0.0f);   // we handle feedback via matrix
            fdn[i].setAM(1.0f);
            fdn[i].setDelay(baseMs[i] * 0.001f);
        }

        // Damping filters inside the loop
        // OnePole is used elsewhere in your code; we’ll use it as simple low-pass damping.
        for (int i = 0; i < FDN; ++i)
            loopLP[i].setFreq(6000.0f);

        // Output tone shaping
        postLP.setFreq(9000.0f);
    }

    // --------------------------
    // Public controls
    // --------------------------
    void setMix(float m)        { mix = std::clamp(m, 0.0f, 1.0f); }
    void setWidth(float w)      { width = std::clamp(w, 0.0f, 1.0f); }

    void setPreDelay(float sec)
    {
        preDelay.setMM(1.0f);
        preDelay.setFM(0.0f);
        preDelay.setAM(1.0f);
        preDelay.setDelay(std::clamp(sec, 0.0f, 0.200f));
    }

    // Size mainly scales delay lengths and early reflection spacing.
    void setSize(float s)
    {
        size = std::clamp(s, 0.0f, 1.0f);

        // Scale FDN base delays 0.6x .. 2.2x
        float scale = 0.6f + size * (2.2f - 0.6f);
        const float baseMs[FDN] = { 29.7f, 37.1f, 41.1f, 53.3f };
        for (int i = 0; i < FDN; ++i)
            fdn[i].setDelay((baseMs[i] * scale) * 0.001f);
    }

    // Decay maps to feedback gain (RT60-ish). Keep under 0.99 for stability.
    void setDecay(float d)
    {
        decay = std::clamp(d, 0.0f, 1.0f);
        // 0..1 -> 0.2..0.92 (musical) ; you can expose "infinite" separately.
        feedback = 0.20f + decay * (0.92f - 0.20f);
    }

    // Damping controls loop LPF cutoff (lower = darker, shorter HF decay)
    void setDamping(float d)
    {
        damping = std::clamp(d, 0.0f, 1.0f);
        // 0..1 -> 1.2 kHz .. 12 kHz
        float fc = 1200.0f + damping * (12000.0f - 1200.0f);
        for (int i = 0; i < FDN; ++i)
            loopLP[i].setFreq(fc);
    }

    // Post tone (overall brightness)
    void setTone(float t)
    {
        tone = std::clamp(t, 0.0f, 1.0f);
        // 0..1 -> 2.5 kHz .. 16 kHz
        float fc = 2500.0f + tone * (16000.0f - 2500.0f);
        postLP.setFreq(fc);
    }

    void setDrive(float d) { drive = std::max(0.0f, d); }

    void setModRate(float hz)   { modRate.set(std::max(0.0f, hz)); }
    void setModDepth(float sec) { modDepth.set(std::max(0.0f, sec)); }

    // Optional: change early reflection patterns by preset
    void setEarlyPatternRoom()
    {
        // 8-tap small room-ish pattern
        // NOTE: MultitapDelay uses seconds per tap in your wrapper.
        static constexpr float taps[8] = {
            0.008f, 0.011f, 0.015f, 0.019f,
            0.024f, 0.031f, 0.039f, 0.048f
        };
        for (unsigned i = 0; i < 8; ++i) early.setTapTime(i, taps[i]);
    }

    void setEarlyPatternPlate()
    {
        // slightly denser early cluster (plate-like "slap cluster")
        static constexpr float taps[8] = {
            0.004f, 0.006f, 0.009f, 0.012f,
            0.016f, 0.021f, 0.027f, 0.034f
        };
        for (unsigned i = 0; i < 8; ++i) early.setTapTime(i, taps[i]);
    }

    // --------------------------
    // Processing
    // --------------------------
    float process(float input) override
    {
        // mono wrapper -> return mid of stereo
        auto lr = processStereo(input);
        return 0.5f * (lr.first + lr.second);
    }

    std::pair<float, float> processStereo(float input)
    {
        // 1) Pre-emphasis / input drive (optional; keep subtle)
        float x = softClip(input, drive);

        // 2) Pre-delay (fully wet block)
        float xPred = preDelay.update(x);

        // 3) Early reflections (multitap)
        // MultitapDelay::process() is mono; we use it as ER mono then stereoize later.
        float er = early.process(xPred);

        // 4) Primary diffusion (smear early energy)
        float d1 = diffuserA.process(er);

        // 5) FDN: 4 delay lines with feedback matrix + loop damping + tiny modulation
        // Build modulation value
        advanceLFO();
        const float m = std::sin(lfoPhase * TAU) * modDepth.process(); // seconds

        // Read current line outputs
        float y[FDN];
        for (int i = 0; i < FDN; ++i)
        {
            // Tiny per-line modulation offsets so they don't move together
            float mi = m * (0.7f + 0.2f * i);
            applyLineMod(i, mi);
            y[i] = fdn[i]._delay.read(); // NOTE: fdn is ModDelay; _delay is protected in base.
                                         // If _delay is protected, ModReverb can't access it.
                                         // If not accessible, add a public readRaw()/writeRaw() in ModDelay, or
                                         // implement a ModDelayFDNLine wrapper. See note below.
        }

        // Mix outputs for injection via a simple Householder/Hadamard-like matrix
        // This keeps energy and avoids obvious repeats.
        float v[FDN];
        fdnMixMatrix(y, v);

        // Loop: damping + saturation + write
        float inToFDN = d1; // feed ER+diffusion into the tank
        for (int i = 0; i < FDN; ++i)
        {
            float loop = v[i] * feedback;

            loop = loopLP[i].process(loop);
            loop = softClip(loop, drive * 0.7f);

            // Write input + feedback into the delay line
            fdn[i]._delay.write(inToFDN + loop);
        }

        // Sum tank output
        float tank = 0.0f;
        for (int i = 0; i < FDN; ++i)
            tank += y[i];
        tank *= (1.0f / (float)FDN);

        // 6) Secondary diffusion (soften metallicness)
        float d2 = diffuserB.process(tank);

        // 7) Post tone
        float wet = postLP.process(d2);

        // 8) Stereo width (simple decorrelation)
        // Use a tiny Haas-style offset derived from LFO phase for subtle movement
        float wetL, wetR;
        stereoize(wet, wetL, wetR);

        // 9) Wet/dry mix
        float outL = input * (1.0f - mix) + wetL * mix;
        float outR = input * (1.0f - mix) + wetR * mix;
        return { outL, outR };
    }

    // --------------------------
    // Notes on ModDelay integration
    // --------------------------
    // In the code above, I accessed fdn[i]._delay.read()/write().
    // In your ModDelay, _delay is `protected: Delay _delay;`
    // so ModReverb (not derived from ModDelay) cannot access it.
    //
    // To plug in cleanly, do ONE of these:
    //  (A) Add to ModDelay:
    //      float readRaw() const { return _delay.read(); }
    //      void  writeRaw(float v) { _delay.write(v); }
    //      void  setDelayTimeRaw(float sec) { _delay.setDelay(sec); }
    //
    //  (B) Make a small FDNLine wrapper around Delay directly.
    //
    // Option A keeps your ecosystem consistent and is recommended.

private:
    // ---- Helpers ----
    static constexpr float TAU = 6.28318530718f;

    void advanceLFO()
    {
        float hz = modRate.process();
        lfoPhase += hz / (float)gam::sampleRate();
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
    }

    void applyLineMod(int i, float modSeconds)
    {
        // Convert seconds modulation into ModDelay's multiplicative DM:
        // delayT = baseDelay * (1 + dm)
        // dm ≈ modSeconds/baseDelay
        float base = fdnBaseDelaySec(i);
        float dm = (base > 1e-6f) ? (modSeconds / base) : 0.0f;
        fdn[i].setDM(dm);
        fdn[i]._delay.setDelay(base * (1.0f + dm)); // see access note above
    }

    float fdnBaseDelaySec(int i) const
    {
        // Query current delay time from wrapper
        return fdn[i].getDelay();
    }

    static void fdnMixMatrix(const float in[FDN], float out[FDN])
    {
        // 4x4 Hadamard (energy preserving) scaled by 0.5
        // out = H * in
        const float s = 0.5f;
        out[0] = s * ( in[0] + in[1] + in[2] + in[3]);
        out[1] = s * ( in[0] - in[1] + in[2] - in[3]);
        out[2] = s * ( in[0] + in[1] - in[2] - in[3]);
        out[3] = s * ( in[0] - in[1] - in[2] + in[3]);
    }

    void stereoize(float wetMono, float& L, float& R)
    {
        // Simple width: modulate a tiny decorrelation delay on one side.
        // If you want: swap this for your Haas widener or StereoDoubler module.
        float side = wetMono * (0.25f + 0.75f * width);
        float mid  = wetMono;

        // crude L/R from mid/side
        L = mid + side * 0.5f;
        R = mid - side * 0.5f;
    }

    // ---- Members ----
    // Pre stage
    float drive = 0.05f;

    // Predelay (reuse ModDelay as a pure wet delay)
    ModDelay preDelay;

    // Early reflections
    MultitapDelay early;

    // Diffusion
    ModDiffuser diffuserA;
    ModDiffuser diffuserB;

    // FDN tank
    std::array<ModDelay, FDN> fdn;
    std::array<OnePole, FDN>  loopLP;

    // Post tone
    OnePole postLP;

    // Modulation
    Modulator modRate;   // Hz
    Modulator modDepth;  // seconds
    float lfoPhase = 0.0f;

    // User params
    float mix = 0.35f;
    float width = 1.0f;

    float size = 0.65f;
    float decay = 0.65f;
    float damping = 0.55f;
    float tone = 0.6f;

    float feedback = 0.75f;
};
