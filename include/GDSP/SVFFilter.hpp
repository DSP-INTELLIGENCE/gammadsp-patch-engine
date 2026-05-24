// SVFFilter.hpp
#pragma once
#include <cmath>
#include <algorithm>

class SVFFilter
{
public:
    explicit SVFFilter(float sampleRate = 48000.0f)
    : fs(std::max(1.0f, sampleRate))
    {
        reset();
        // sensible defaults
        setMix(0.0f, 0.0f, 1.0f);   // LP
        setCutoff(1000.0f);
        setQ(0.707f);
        // initialize coeffs immediately
        updateTargets();
        g = gTarget;
        k = kTarget;
        interpRemaining = -1;
    }

    void setSampleRate(float sampleRate)
    {
        fs = std::max(1.0f, sampleRate);
        // Recompute targets at new SR and ramp (or snap if you prefer)
        updateTargets();
        beginInterpolation();
    }

    void setCutoff(float hz)
    {
        cutoff = hz;
        updateTargets();
        beginInterpolation();
    }

    void setQ(float q)
    {
        Q = q;
        updateTargets();
        beginInterpolation();
    }

    // Mix coefficients for outputs. Any combination allowed.
    // (Typical: LP=(0,0,1), BP=(0,1,0), HP=(1,0,0))
    void setMix(float hp, float bp, float lp)
    {
        mixHP = hp;
        mixBP = bp;
        mixLP = lp;
    }

    // Optional: set interpolation length in samples (0 = immediate)
    void setInterpSamples(int n)
    {
        interpLen = std::max(0, n);
    }

    void reset()
    {
        s1 = 0.0f;
        s2 = 0.0f;
        // keep coeffs as-is; state reset only
        flushDenormals();
    }

    // One-sample process
    float process(float x)
    {
        // Basic input safety (prevents NaN poisoning)
        if (!std::isfinite(x)) x = 0.0f;

        // Coefficient interpolation (linear in g,k)
        if (interpRemaining > 0)
        {
            g += gStep;
            k += kStep;

            if (--interpRemaining == 0)
            {
                // snap exactly to targets
                g = gTarget;
                k = kTarget;
                interpRemaining = -1; // inactive
                gStep = 0.0f;
                kStep = 0.0f;
            }
        }

        // ZDF / TPT SVF core (Simper/Zavalishin form)
        // a = 1 / (1 + g*(g+k))
        const float a  = 1.0f / (1.0f + g * (g + k));
        const float v1 = a * (s1 + g * (x - s2));
        const float v2 = s2 + g * v1;

        const float hp = x - k * v1 - v2;
        const float bp = v1;
        const float lp = v2;

        // Integrator state updates
        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;

        // Denormal protection (important for SVFs)
        flushDenormals();

        // Output mix
        const float y = mixHP * hp + mixBP * bp + mixLP * lp;
        return std::isfinite(y) ? y : 0.0f;
    }

private:
    // --- parameters (user) ---
    float fs = 48000.0f;
    float cutoff = 1000.0f; // Hz
    float Q = 0.707f;

    float mixHP = 0.0f, mixBP = 0.0f, mixLP = 1.0f;

    // --- SVF state ---
    float s1 = 0.0f;
    float s2 = 0.0f;

    // --- coefficients (runtime + targets) ---
    float g = 0.0f;
    float k = 0.0f;

    float gTarget = 0.0f;
    float kTarget = 0.0f;

    float gStep = 0.0f;
    float kStep = 0.0f;

    int interpLen = 64;        // default ramp length
    int interpRemaining = -1;  // -1 inactive, >0 ramping

private:
    static inline float clampf(float x, float lo, float hi)
    {
        return (x < lo) ? lo : (x > hi ? hi : x);
    }

    void updateTargets()
    {
        // Practical clamps (safe + musical)
        const float nyqSafe = 0.45f * fs;       // avoid tan() blow-up near Nyquist
        const float f = clampf(cutoff, 1.0f, nyqSafe);
        const float q = clampf(Q, 0.1f, 30.0f); // keep resonance sane & stable

        // TPT mapping
        const float wd = float(M_PI) * (f / fs);
        float gt = std::tan(wd);
        float kt = 0.5f / q;

        // Safety: if something goes weird, hold last good
        if (!std::isfinite(gt) || gt < 0.0f) gt = gTarget;
        if (!std::isfinite(kt) || kt < 0.0f) kt = kTarget;

        gTarget = gt;
        kTarget = kt;
    }

    void beginInterpolation()
    {
        // If interpLen==0, jump immediately (useful for init or offline)
        if (interpLen <= 0)
        {
            g = gTarget;
            k = kTarget;
            interpRemaining = -1;
            gStep = 0.0f;
            kStep = 0.0f;
            return;
        }

        // If targets are already current, do nothing
        const float dg = gTarget - g;
        const float dk = kTarget - k;
        if (std::fabs(dg) < 1e-12f && std::fabs(dk) < 1e-12f)
            return;

        interpRemaining = interpLen;
        const float invN = 1.0f / float(interpLen);
        gStep = dg * invN;
        kStep = dk * invN;
    }

    inline void flushDenormals()
    {
        // Branchy zeroing is fine and predictable.
        // (You can switch to add/sub 1e-20f if you prefer branchless.)
        if (std::fabs(s1) < 1e-15f) s1 = 0.0f;
        if (std::fabs(s2) < 1e-15f) s2 = 0.0f;
    }
};
