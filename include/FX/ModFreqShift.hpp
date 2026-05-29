#pragma once
#include <algorithm>
#include <cmath>
#include "FreqShift.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"

class ModFreqShift : public Function
{
public:
    ModFreqShift(float shiftHz = 0.0f)
    : mShift(shiftHz),
      baseShift(shiftHz)
    {
        setIM(1.0f);
        setAM(1.0f);
        setMix(1.0f);     // default fully wet (freq shift is usually used wet)
        setFB(0.0f);
        setShiftDepth(0.0f);
    }

    // ---------------- Base controls ----------------
    void setShift(float hz) { baseShift = hz; }
    void setShiftDepth(float d) { depthShift = std::max(0.0f, d); } // in "fraction of base" or absolute? (see below)
    void setFB(float v) { baseFB = std::clamp(v, -0.999f, 0.999f); }
    void setMix(float v) { baseMix = std::clamp(v, 0.0f, 1.0f); }

    // ---------------- Modulation inputs ----------------
    // Mod signals should be in [-1,1] unless noted.
    void setSM(float v)   { sm.set(v); }    // shift modulation signal
    void setFBM(float v)  { fbm.set(v); }   // feedback modulation signal
    void setMIXM(float v) { mixm.set(v); }  // mix modulation signal

    // ---------------- Gain stages ----------------
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    // Optional: clamp shift to avoid insane settings
    void setMaxAbsShift(float hz) { maxAbsShift = std::max(0.0f, hz); }

    float process(float input) override
    {
        const float _im   = im.process();
        const float _am   = am.process();

        const float _sm   = sm.process();     // [-1,1]
        const float _fbm  = fbm.process();    // [-1,1]
        const float _mixm = mixm.process();   // [-1,1]

        // --- Compute modulated parameters ---
        // Shift modulation model:
        // shift = baseShift + (depthShift * baseShift) * sm
        // This mirrors your preferred "base + depth*base*signal" style.
        float shift = baseShift + (depthShift * baseShift) * _sm;

        if (maxAbsShift > 0.0f)
            shift = std::clamp(shift, -maxAbsShift, maxAbsShift);

        // Feedback modulation model: base + base*modSignal
        float fb = baseFB + baseFB * _fbm;
        fb = std::clamp(fb, -0.999f, 0.999f);

        // Mix modulation model: base + base*modSignal
        float mix = baseMix + baseMix * _mixm;
        mix = std::clamp(mix, 0.0f, 1.0f);

        // --- Apply shift ---
        // Update underlying FreqShift parameter per-sample (OK for audio-rate modulation).
        mShift.setShift(shift);

        // --- Feedback topology (1-sample delayed feedback) ---
        float in = input * _im + lastY * fb;

        float wet = mShift.process(in);

        // --- Wet/dry mix & output gain ---
        float out = input * (1.0f - mix) + wet * mix;
        out *= _am;

        lastY = wet; // feedback uses wet path (usually best for freq shifting)
        return out;
    }

private:
    FreqShift mShift;

    // Base params
    float baseShift   = 0.0f;  // Hz
    float depthShift  = 0.0f;  // unitless depth (0..inf, typically 0..1)
    float baseFB      = 0.0f;
    float baseMix     = 1.0f;

    // Safety
    float maxAbsShift = 0.0f;  // 0 disables clamp

    // Modulators (signals)
    Modulator sm;    // shift mod signal
    Modulator fbm;   // feedback mod signal
    Modulator mixm;  // mix mod signal
    Modulator im;    // input gain
    Modulator am;    // output gain

    float lastY = 0.0f;
};
