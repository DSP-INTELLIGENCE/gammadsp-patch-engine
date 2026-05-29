#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "GDSP_ModAllPass2.hpp"
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

class AutonomousPhaseEngine : public Function
{
public:
    AutonomousPhaseEngine(int stages = 10)
    : baseFreq(240.f),
      baseWidth(180.f),
      mix(0.75f),
      fb(0.82f),
      drive(1.8f),
      dispersion(0.75f),
      autonomy(0.85f),
      wander(0.65f),
      chaos(0.35f),
      inject(0.0025f)
    {
        setRate(0.08f);
        setDepth(0.85f);
        setEnvAmount(0.20f);
        setIM(1.f);
        setAM(1.f);
        setSeed(0xC0FFEEu);

        chain.reserve(std::max(1, stages));
        for(int i = 0; i < stages; ++i)
            chain.emplace_back(baseFreq, baseWidth);
    }

    // ---------- Core musical controls ----------
    void setMix(float m)        { mix = std::clamp(m, 0.f, 1.f); }
    void setFeedback(float f)   { fb  = std::clamp(f, -0.995f, 0.995f); }
    void setDrive(float d)      { drive = std::max(0.f, d); }

    void setBaseFreq(float f)   { baseFreq  = std::max(40.f, f); }
    void setBaseWidth(float w)  { baseWidth = std::max(1.f,  w); }

    // 0..1: spread across stages + group-delay richness
    void setDispersion(float d) { dispersion = std::clamp(d, 0.f, 1.f); }

    // 0..1: how much it keeps moving without input
    void setAutonomy(float a)   { autonomy = std::clamp(a, 0.f, 1.f); }

    // 0..1: slow random walk amount (macro evolution)
    void setWander(float w)     { wander = std::clamp(w, 0.f, 1.f); }

    // 0..1: nonlinear “life” / instability (careful above ~0.7)
    void setChaos(float c)      { chaos = std::clamp(c, 0.f, 1.f); }

    // tiny internal excitation (0..0.02 recommended)
    void setInject(float i)     { inject = std::clamp(i, 0.f, 0.05f); }

    // Motion controls (like your phasers)
    void setRate(float r)       { rate.set(std::max(0.001f, r)); }
    void setDepth(float d)      { depth = std::clamp(d, 0.f, 1.f); }
    void setEnvAmount(float e)  { envAmt = std::clamp(e, 0.f, 1.f); }

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    // Stable envelope time (seconds)
    void setEnvTime(float t)
    {
        float sr = (float)gam::sampleRate();
        t = std::max(0.001f, t);
        envCoef = std::exp(-1.f / (t * sr));
    }

    void setSeed(uint32_t s) { rng = (s ? s : 1u); }

    void reset()
    {
        env = 0.f; last = 0.f;
        phase = 0.f;
        drift = 0.f;
        rw = 0.f;
        x1 = 0.17f; // chaotic state
        for(auto& s : chain) s.reset();
    }

    float process(float x) override
    {
        const float sr = (float)gam::sampleRate();

        // --- Envelope follower (input *or* self output can drive it subtly)
        float level = std::fabs(x);
        env = level + envCoef * (env - level);

        // --- Internal slow LFO phase (continuous)
        float r = rate.process();                    // treat as LFO signal source
        phase += (0.05f + 0.35f * std::fabs(r)) / sr; // always moves (even if r is flat)
        if(phase >= 1.f) phase -= 1.f;

        float lfo = std::sin(phase * 6.2831853f);

        // --- Random walk (very slow “ecosystem drift”)
        // step ~ noise filtered into a random walk
        float n = white() * 0.0025f;
        rw = std::clamp(rw + n * (0.2f + 2.0f * wander), -1.f, 1.f);

        // --- Chaotic modulator (logistic map)
        // When chaos > 0, this generates organic, non-repeating motion.
        x1 = x1 + chaos * 0.25f * (4.f * x1 * (1.f - x1) - x1);
        x1 = std::clamp(x1, 0.0001f, 0.9999f);
        float cha = (x1 * 2.f - 1.f); // [-1,1]

        // --- “Autonomous drive” source: tiny injection + a bit of recirculated energy
        // This is what keeps it alive with no input.
        float excite = inject * (0.6f * white() + 0.4f * std::sin(phase * 6.2831853f * 0.5f));

        // Blend input and self excitation based on autonomy.
        float inMix = 1.f - autonomy;
        float src = (x * inMix) + (excite + 0.08f * last) * autonomy;

        // --- Combined motion controls
        // lfo = cyclical, env = performance, rw = long drift, cha = organic chaos
        float motion = (lfo * depth)
                     + (env * envAmt)
                     + (rw  * 0.60f * wander)
                     + (cha * 0.45f * chaos);

        // Center frequency: exponential mapping (musical)
        float center = baseFreq * std::pow(2.f, motion * 3.f);
        center = std::clamp(center, 40.f, 0.45f * sr);

        // Dispersion width: stays alive even in silence (rw + cha drive it)
        float width = baseWidth * (1.f + dispersion * (0.8f + 0.4f * motion))
                    + 900.f * dispersion * (0.35f * wander * rw + 0.25f * chaos * cha);
        width = std::clamp(width, 1.f, 8000.f);

        // --- Nonlinear feedback core (stability + character)
        float fbSig = last * fb;
        fbSig = std::tanh(fbSig * drive);

        float sig = src * im.process() + fbSig;

        // --- Dispersive multi-stage AP network
        const int N = (int)chain.size();
        for(int i = 0; i < N; ++i)
        {
            // stage spread in log-frequency: richer “3D” motion
            float t = (N > 1) ? (float)i / (float)(N - 1) : 0.f;
            float centered = (t - 0.5f) * 2.f;                 // -1..1
            float stageMul = std::pow(2.f, centered * (0.18f + 0.55f * dispersion));

            float fc = std::clamp(center * stageMul, 40.f, 0.45f * sr);

            // slightly different width per stage for “smear”
            float w = width * (1.f + 0.12f * dispersion * std::sin(11.7f * (float)i + 3.1f * cha));
            w = std::clamp(w, 1.f, 8000.f);

            chain[i].setCutoff(fc);
            chain[i].setWidth(w);
            sig = chain[i].process(sig);
        }

        // keep energy bounded & interesting
        sig = std::tanh(sig * (1.0f + 0.6f * chaos));

        last = sig;

        float out = x * (1.f - mix) + sig * mix;
        out *= am.process();
        return out;
    }

private:
    // Tiny PRNG: fast, deterministic, no deps
    float white()
    {
        // xorshift32
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        // map to [-1, 1]
        uint32_t v = rng;
        float f = (float)(v & 0x00FFFFFFu) / (float)0x00800000u; // ~[0,2)
        return f - 1.f;
    }

private:
    std::vector<ModAllPass2> chain;

    float baseFreq;
    float baseWidth;

    float mix;
    float fb;
    float drive;

    float dispersion; // 0..1
    float autonomy;   // 0..1 (0 = normal phaser, 1 = organism)
    float wander;     // 0..1
    float chaos;      // 0..1
    float inject;     // small excitation

    Modulator rate;
    Modulator im, am;

    float depth  = 0.85f;
    float envAmt = 0.20f;

    float env = 0.f;
    float envCoef = 0.0f;

    float phase = 0.f;
    float drift = 0.f; // reserved
    float rw = 0.f;

    float last = 0.f;

    // chaotic state (0..1)
    float x1 = 0.17f;

    uint32_t rng = 1u;
};
