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
    }

    inline float process(float x)
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
// SVF Filter with Morphing + Saturated Feedback
// =============================================================
class SVFSaturationFilter
{
public:
    explicit SVFSaturationFilter(float sampleRate = 48000.0f)
    {
        setSampleRate(sampleRate);
        setMode(0.0f);
        setCutoff(1000.0f);
        setQ(0.707f);
        updateTargets();
        g = gTarget; k = kTarget;
    }

    void reset() { s1 = s2 = 0.0f; }

    void setSampleRate(float sr)
    {
        fs = std::max(1.0f, sr);
        updateTargets();
        beginInterpolation();
    }

    void setCutoff(float hz) { cutoff = hz; updateTargets(); beginInterpolation(); }
    void setQ(float q)       { Q = q; updateTargets(); beginInterpolation(); }

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

        const float a  = 1.0f / (1.0f + g * (g + k));
        const float v1 = a * (s1 + g * (x - s2));
        const float v2 = s2 + g * v1;

        float fb = sat.process(k * v1);  // <<< feedback-only saturation

        const float hp = x - fb - v2;
        const float bp = v1;
        const float lp = v2;

        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;

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

    void updateTargets()
    {
        float f = std::clamp(cutoff, 1.0f, 0.45f * fs);
        float q = std::clamp(Q, 0.1f, 30.0f);

        gTarget = std::tan(float(M_PI) * f / fs);
        kTarget = 0.5f / q;
    }

    void beginInterpolation()
    {
        interpRemaining = interpLen;
        gStep = (gTarget - g) / float(interpLen);
        kStep = (kTarget - k) / float(interpLen);
    }
};
