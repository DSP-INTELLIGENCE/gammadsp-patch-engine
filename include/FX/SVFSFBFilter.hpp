#pragma once
#include <cmath>
#include <algorithm>

// =============================================================
// Saturation used ONLY in feedback path (analog style)
// =============================================================
class SVFSaturation
{
public:
    enum Type { SoftClip, Tanh, Asymmetric };

    SVFSaturation() { setType(SoftClip); setDriveDB(0.0f); }

    void setType(Type t) { type = t; }

    void setDriveDB(float dB)
    {
        drive = std::pow(10.0f, dB / 20.0f);
        if (!std::isfinite(drive) || drive < 0.0f) drive = 1.0f;
    }

    inline float process(float x) const
    {
        x *= drive;

        switch (type)
        {
        case SoftClip:   x = x / (1.0f + std::fabs(x)); break;
        case Tanh:      x = std::tanh(x); break;
        case Asymmetric:
            x = (x >= 0.0f) ? (x / (1.0f + x)) : (x / (1.0f - 0.5f * x));
            break;
        }

        return std::isfinite(x) ? x : 0.0f;
    }

private:
    Type  type  = SoftClip;
    float drive = 1.0f;
};

// =============================================================
// SVF Filter with Morphing + Saturated Feedback + Self-Osc Stabilization
// =============================================================
class SVFSFBFilter
{
public:
    explicit SVFSFBFilter(float sampleRate = 48000.0f)
    {
        setSampleRate(sampleRate);
        setMode(0.0f);
        setCutoff(1000.0f);
        setQ(0.707f);

        // self-osc defaults (tunable)
        setSelfOscStabilization(true);
        setOscTarget(0.25f);
        setPitchCompAmount(0.03f);

        updateTargets();
        g = gTarget; k = kTarget;
        interpRemaining = 0;
        agc = 1.0f;
        env = 0.0f;
    }

    void reset()
    {
        s1 = s2 = 0.0f;
        env = 0.0f;
        agc = 1.0f;
    }

    void setSampleRate(float sr)
    {
        fs = std::max(1.0f, sr);
        updateTargets();
        beginInterpolation();
        // update follower rates from SR
        updateFollowerRates();
    }

    void setCutoff(float hz) { cutoff = hz; updateTargets(); beginInterpolation(); }
    void setQ(float q)       { Q = q;     updateTargets(); beginInterpolation(); }

    void setDriveDB(float dB) { sat.setDriveDB(dB); }
    void setSaturationType(SVFSaturation::Type t) { sat.setType(t); }

    // 0 = LP → 0.5 = BP → 1 = HP
    void setMode(float m)
    {
        mode = std::clamp(m, 0.0f, 1.0f);

        if (mode < 0.5f)
        {
            float t = mode * 2.0f;
            mixLP = std::cos(0.5f * float(M_PI) * t);
            mixBP = std::sin(0.5f * float(M_PI) * t);
            mixHP = 0.0f;
        }
        else
        {
            float t = (mode - 0.5f) * 2.0f;
            mixLP = 0.0f;
            mixBP = std::cos(0.5f * float(M_PI) * t);
            mixHP = std::sin(0.5f * float(M_PI) * t);
        }
    }

    // -----------------------------
    // Self-osc pitch stabilization controls
    // -----------------------------
    void setSelfOscStabilization(bool enabled) { stabilize = enabled; }

    // Target BP amplitude (roughly 0.15..0.4 are common)
    void setOscTarget(float target)
    {
        oscTarget = std::clamp(target, 1e-4f, 2.0f);
    }

    // Small amplitude->pitch compensation (0..0.08 typical)
    void setPitchCompAmount(float amt)
    {
        pitchComp = std::clamp(amt, 0.0f, 0.2f);
    }

    // Times in seconds: attack ~0.03-0.06s, release ~0.10-0.25s
    void setOscFollowerTimes(float attackSec, float releaseSec)
    {
        envAtkSec = std::clamp(attackSec, 1e-4f, 2.0f);
        envRelSec = std::clamp(releaseSec, 1e-4f, 5.0f);
        updateFollowerRates();
    }

    // AGC speed: how quickly AGC approaches target (0.0002..0.002 typical)
    void setOscAgcRate(float rate)
    {
        agcRate = std::clamp(rate, 0.0f, 0.01f);
    }

    // Clamp AGC range
    void setOscAgcRange(float minGain, float maxGain)
    {
        agcMin = std::clamp(minGain, 0.01f, 100.0f);
        agcMax = std::clamp(maxGain, agcMin, 100.0f);
    }

    float process(float x)
    {
        if (!std::isfinite(x)) x = 0.0f;

        // interpolate coefficients
        if (interpRemaining > 0)
        {
            g += gStep;  k += kStep;
            if (--interpRemaining == 0)
            {
                g = gTarget; k = kTarget;
            }
        }

        // --- optional small pitch compensation via amplitude-dependent g ---
        // Use last env (previous sample) to avoid extra iteration.
        float gUse = g;
        if (stabilize && pitchComp > 0.0f)
        {
            // Increase g slightly with amplitude; counters typical downward pitch drift
            // Clamp to avoid numerical stress.
            const float scale = 1.0f + pitchComp * env;
            gUse = std::min(g * scale, 8.0f); // 8 is conservative; g blows up near Nyq otherwise
        }

        // ZDF SVF core
        const float a  = 1.0f / (1.0f + gUse * (gUse + k));
        const float v1 = a * (s1 + gUse * (x - s2));
        const float v2 = s2 + gUse * v1;

        const float bp = v1;

        // --- envelope follower on BP amplitude ---
        const float bpAbs = std::fabs(bp);
        const float c = (bpAbs > env) ? envAtk : envRel;
        env += c * (bpAbs - env);

        // --- AGC: keep oscillation amplitude steady => stabilizes pitch ---
        if (stabilize)
        {
            const float invEnv = 1.0f / std::max(env, 1e-6f);
            const float desired = oscTarget * invEnv; // gain needed to hit target
            agc += agcRate * (desired - agc);
            agc = std::clamp(agc, agcMin, agcMax);
        }
        else
        {
            agc = 1.0f;
        }

        // feedback-only saturation (analog style) + AGC scaling
        float fbIn = (k * v1) * agc;
        float fb = sat.process(fbIn);

        const float hp = x - fb - v2;
        const float lp = v2;

        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;

        // denormal protection
        if (std::fabs(s1) < 1e-15f) s1 = 0.0f;
        if (std::fabs(s2) < 1e-15f) s2 = 0.0f;

        const float y = mixHP * hp + mixBP * bp + mixLP * lp;
        return std::isfinite(y) ? y : 0.0f;
    }

private:
    SVFSaturation sat;

    float fs = 48000.0f;
    float cutoff = 1000.0f;
    float Q = 0.707f;

    float s1 = 0.0f, s2 = 0.0f;
    float g = 0.0f, k = 0.0f;
    float gTarget = 0.0f, kTarget = 0.0f;
    float gStep = 0.0f, kStep = 0.0f;

    int interpLen = 64;
    int interpRemaining = 0;

    float mode = 0.0f;
    float mixLP = 1.0f, mixBP = 0.0f, mixHP = 0.0f;

    // --- self-osc stabilization state/params ---
    bool  stabilize = true;
    float oscTarget = 0.25f;

    float env = 0.0f;
    float agc = 1.0f;

    float envAtkSec = 0.04f;
    float envRelSec = 0.15f;
    float envAtk = 0.0f; // per-sample coeff
    float envRel = 0.0f; // per-sample coeff

    float agcRate = 0.0005f;
    float agcMin = 0.25f;
    float agcMax = 4.0f;

    float pitchComp = 0.03f;

    void updateFollowerRates()
    {
        // One-pole coefficients: y += c*(x-y)
        // c = 1 - exp(-1/(tau*fs))
        const float fsC = std::max(1.0f, fs);
        const float atkTau = std::max(1e-4f, envAtkSec);
        const float relTau = std::max(1e-4f, envRelSec);

        envAtk = 1.0f - std::exp(-1.0f / (atkTau * fsC));
        envRel = 1.0f - std::exp(-1.0f / (relTau * fsC));

        // guard
        if (!std::isfinite(envAtk) || envAtk < 0.0f) envAtk = 0.001f;
        if (!std::isfinite(envRel) || envRel < 0.0f) envRel = 0.0001f;
    }

    void updateTargets()
    {
        float f = std::clamp(cutoff, 1.0f, 0.45f * fs);
        float q = std::clamp(Q, 0.1f, 30.0f);

        gTarget = std::tan(float(M_PI) * f / fs);
        kTarget = 0.5f / q;

        if (!std::isfinite(gTarget) || gTarget < 0.0f) gTarget = g;
        if (!std::isfinite(kTarget) || kTarget < 0.0f) kTarget = k;
    }

    void beginInterpolation()
    {
        const int N = std::max(1, interpLen);
        interpRemaining = N;
        gStep = (gTarget - g) / float(N);
        kStep = (kTarget - k) / float(N);
    }
};
