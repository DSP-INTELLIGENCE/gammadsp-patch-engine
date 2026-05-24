// ModRBJ.hpp
#pragma once
#include <algorithm>
#include <cmath>
#include "Engine.hpp"
#include "Parameters.hpp"
#include "RBJFilter.hpp"   // your RBJFilter template

// ModRBJ: wraps RBJFilter with your "base + depth*base*signal" modulation style
// - FM: cutoff modulation (Hz), depth scales relative to baseFreq
// - QM: Q modulation, depth scales relative to baseQ
// - GM: gain dB modulation (for PEAKING / shelves), depth scales relative to baseGainDb
// - SM: shelf slope modulation, depth scales relative to baseSlope
// - IM/AM: input/output gains
// - FB: simple 1-sample feedback around the filter
// - MIX: wet/dry mix
//
// Notes:
// 1) For GM, if baseGainDb is 0, modulation won't do anything (since base*depth*sig = 0).
//    If you want gain-mod always available, switch GM mapping to "base + depth*sig" (absolute).
// 2) This wrapper is audio-rate safe but you may prefer to update coefficients only when params ramp.

template<class Sample=float>
class ModRBJFilter : public Function
{
public:
    using Type = typename RBJFilter<Sample>::Type;

    ModRBJFilter(Sample sampleRate,
           Type type = Type::LPF,
           Sample initFreqHz = Sample(1000),
           Sample initQ      = Sample(0.707),
           Sample initGainDb = Sample(0),
           Sample initSlope  = Sample(1))
    : mSR(sampleRate),
      mFilter(sampleRate, type, initFreqHz, initQ, initGainDb, initSlope)
    {
        // Base defaults
        setType(type);
        setFreq(initFreqHz);
        setQ(initQ);
        setGainDb(initGainDb);
        setSlope(initSlope);

        // Mod depths defaults
        setFDepth(0.0f);
        setQDepth(0.0f);
        setGDepth(0.0f);
        setSDepth(0.0f);

        // Signals defaults
        setFM(0.0f);
        setQM(0.0f);
        setGM(0.0f);
        setSM(0.0f);

        // Mix/gains
        setMix(1.0f);
        setFB(0.0f);
        setIM(1.0f);
        setAM(1.0f);
    }

    // ---------------- Base controls (slow knobs) ----------------
    void setType(Type t) { mType = t; mFilter.setType(t); }

    void setFreq(Sample hz)    { baseFreqHz = std::max<Sample>(Sample(1), hz); }
    void setQ(Sample q)        { baseQ      = std::max<Sample>(Sample(0.001), q); }
    void setGainDb(Sample db)  { baseGainDb = db; }
    void setSlope(Sample s)    { baseSlope  = std::max<Sample>(Sample(0.001), s); }

    void setMix(Sample m) { baseMix = std::clamp<Sample>(m, Sample(0), Sample(1)); }
    void setFB(Sample f)  { baseFB  = std::clamp<Sample>(f, Sample(-0.99), Sample(0.99)); }

    void reset()
    {
        mFilter.reset();
        last = Sample(0);
    }

    // ---------------- Depth controls (how strongly the mod signals affect params) ----------------
    // Depths are unitless multipliers applied to the "base * signal" style.
    void setFDepth(float d) { depthF = std::max(0.0f, d); }
    void setQDepth(float d) { depthQ = std::max(0.0f, d); }
    void setGDepth(float d) { depthG = std::max(0.0f, d); }
    void setSDepth(float d) { depthS = std::max(0.0f, d); }

    // ---------------- Modulation signals (typically [-1,1]) ----------------
    void setFM(float v)   { fm.set(v); }     // cutoff mod signal
    void setQM(float v)   { qm.set(v); }     // Q mod signal
    void setGM(float v)   { gm.set(v); }     // gain dB mod signal
    void setSM(float v)   { sm.set(v); }     // slope mod signal

    void setMIXM(float v) { mixm.set(v); }   // mix mod signal
    void setFBM(float v)  { fbm.set(v); }    // feedback mod signal

    // ---------------- Gain stages ----------------
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    // ---------------- Processing ----------------
    Sample process(Sample input) override
    {
        // --- read mod signals ---
        const Sample _fm   = (Sample)fm.process();    // [-1,1]
        const Sample _qm   = (Sample)qm.process();    // [-1,1]
        const Sample _gm   = (Sample)gm.process();    // [-1,1]
        const Sample _sm   = (Sample)sm.process();    // [-1,1]
        const Sample _mixm = (Sample)mixm.process();  // [-1,1]
        const Sample _fbm  = (Sample)fbm.process();   // [-1,1]

        const Sample _im = (Sample)im.process();
        const Sample _am = (Sample)am.process();

        // --- compute modulated parameters (base + depth*base*signal) ---
        Sample freqHz = baseFreqHz + (Sample(depthF) * baseFreqHz) * _fm;
        Sample q      = baseQ      + (Sample(depthQ) * baseQ)      * _qm;
        Sample gainDb = baseGainDb + (Sample(depthG) * baseGainDb) * _gm;
        Sample slope  = baseSlope  + (Sample(depthS) * baseSlope)  * _sm;

        // --- safety clamps ---
        const Sample sr = mSR;
        freqHz = std::clamp(freqHz, Sample(1), Sample(0.45) * sr);
        q      = std::max(q, Sample(0.001));
        slope  = std::max(slope, Sample(0.001));
        // gainDb can be any, but you may clamp for sanity if desired:
        // gainDb = std::clamp(gainDb, Sample(-48), Sample(48));

        // --- apply to filter (smoothed internally) ---
        // Note: RBJFilter already clamps/smooths, this is just consistent.
        mFilter.setType(mType);
        mFilter.setFreq(freqHz);
        mFilter.setQ(q);
        mFilter.setGainDb(gainDb);
        mFilter.setShelfSlope(slope);

        // --- mix and feedback ---
        Sample mix = baseMix + baseMix * _mixm;
        mix = std::clamp(mix, Sample(0), Sample(1));

        Sample fb = baseFB + baseFB * _fbm;
        fb = std::clamp(fb, Sample(-0.99), Sample(0.99));

        // 1-sample feedback around wet path
        const Sample in = input * _im + last * fb;

        const Sample wet = mFilter.process(in);
        Sample out = input * (Sample(1) - mix) + wet * mix;
        out *= _am;

        last = wet;
        return out;
    }

    void run(const Sample* in, Sample* out, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }

private:
    Sample mSR;
    Type   mType;

    RBJFilter<Sample> mFilter;

    // base params
    Sample baseFreqHz = Sample(1000);
    Sample baseQ      = Sample(0.707);
    Sample baseGainDb = Sample(0);
    Sample baseSlope  = Sample(1);

    Sample baseMix = Sample(1);
    Sample baseFB  = Sample(0);

    // depths
    float depthF = 0.0f;
    float depthQ = 0.0f;
    float depthG = 0.0f;
    float depthS = 0.0f;

    // modulators (signals)
    Modulator fm, qm, gm, sm;
    Modulator mixm, fbm;
    Modulator im, am;

    Sample last = Sample(0);
};
