// ModularDelay.hpp
#pragma once
#include <cmath>
#include <algorithm>
#include "Delay.hpp"
#include "Parameters.hpp"

inline float gdsp_clampf(float x, float lo, float hi){
    return std::max(lo, std::min(hi, x));
}

// -----------------------------
// Lightweight loop-friendly filters
// (replace with Filters.hpp versions if you prefer)
// -----------------------------
struct OnePoleLP {
    float a = 0.0f;
    float y = 0.0f;

    void setCutoffHz(float hz){
        float sr = (float)gam::sampleRate();
        hz = gdsp_clampf(hz, 5.0f, 0.49f * sr);
        // crude mapping (stable + cheap); OK for damping loops
        float x = hz / (0.5f * sr);     // 0..1
        a = gdsp_clampf(x, 0.0005f, 0.9995f);
    }

    float process(float x){
        y += a * (x - y);
        return y;
    }
};

struct OnePoleHP {
    OnePoleLP lp;
    void setCutoffHz(float hz){ lp.setCutoffHz(hz); }
    float process(float x){ return x - lp.process(x); }
};

// Simple tilt tone: split around pivot with 1-pole LP
struct TiltTone {
    OnePoleLP lp;
    float pivotHz = 800.0f;
    float tiltDb  = 0.0f; // negative=dark, positive=bright

    void setPivot(float hz){ pivotHz = hz; lp.setCutoffHz(pivotHz); }
    void setTiltDb(float db){ tiltDb = db; }

    float process(float x){
        float lo = lp.process(x);
        float hi = x - lo;

        // +/- 12 dB max recommended; loop is cumulative!
        float db = gdsp_clampf(tiltDb, -12.0f, 12.0f);
        float gLo = std::pow(10.0f, (-db) / 20.0f);
        float gHi = std::pow(10.0f, ( db) / 20.0f);

        return lo * gLo + hi * gHi;
    }
};

// Soft clipper for loop stability + character
struct SoftClip {
    float drive = 1.0f;   // 1..~6
    float mix   = 1.0f;   // 0..1

    void setDrive(float d){ drive = std::max(0.0f, d); }
    void setMix(float m){ mix = gdsp_clampf(m, 0.0f, 1.0f); }

    float process(float x){
        float in = x * drive;
        float y  = in / (1.0f + std::abs(in));     // very stable
        return in + (y - in) * mix;                // blend
    }
};

// Slow loop energy controller (very transparent)
struct LoopEnergyLimiter {
    float env = 0.0f;
    float release = 0.9996f;  // closer to 1 = slower decay
    float threshold = 1.0f;   // RMS-ish peak guard

    void setRelease(float r){ release = gdsp_clampf(r, 0.9f, 0.99999f); }
    void setThreshold(float t){ threshold = std::max(0.0001f, t); }

    float process(float x){
        float e = std::abs(x);
        env = std::max(e, env * release);

        if(env > threshold){
            float g = threshold / env;
            x *= g;
        }
        return x;
    }
};

// RT60 -> feedback gain mapping (musical)
inline float gdsp_feedback_from_rt60(float delaySec, float rt60Sec){
    if(rt60Sec <= 0.001f) return 0.0f;
    // g = 10^(-3T/RT60)
    return std::pow(10.0f, -3.0f * delaySec / rt60Sec);
}

// -----------------------------
// Modular Delay (mono core)
// -----------------------------
class DFXModularDelay : public Function {
public:
    explicit DFXModularDelay(float maxDelaySeconds = 2.0f,
                          float initDelaySeconds = 0.25f)
    : mDelay(maxDelaySeconds, initDelaySeconds)
    {
        mDelay.setIpolType(ALLPASS); // best default for modulation
        // sensible defaults
        setSmoothingMs(15.0f);
        setDamping(100.0f, 6500.0f);
        setTonePivot(900.0f);
        setToneTiltDb(0.0f);
        setSaturation(1.5f, 0.5f);
        setFeedback01(0.35f);
        setMix01(0.35f);
        setTimeModAmount(0.0f);
        setStability(0.995f, 1.0f, 0.9996f);
    }

    // ---------- user-facing params ----------
    void setDelayTime(float seconds)        { pDelaySec.set(std::max(0.0f, seconds)); }
    void setFeedback01(float fb01)          { pFeedback01.set(gdsp_clampf(fb01, 0.f, 1.f)); }
    void setDecayRT60(float seconds)        { pRT60.set(std::max(0.001f, seconds)); pUseRT60.set(1.0f); }
    void setUseRawFeedback(bool b)          { pUseRT60.set(b ? 0.0f : 1.0f); } // false => RT60 mode

    void setMix01(float mix01)              { pMix01.set(gdsp_clampf(mix01, 0.f, 1.f)); }
    void setInputGain(float g)              { pInGain.set(g); }
    void setWetGain(float g)                { pWetGain.set(g); }
    void setDryGain(float g)                { pDryGain.set(g); }

    // Damping filter (in-loop)
    void setDamping(float hpfHz, float lpfHz){
        pHPFHz.set(hpfHz);
        pLPFHz.set(lpfHz);
    }

    // Tone filter (in-loop)
    void setTonePivot(float hz)             { pTonePivotHz.set(hz); }
    void setToneTiltDb(float db)            { pToneTiltDb.set(db); } // -12..+12 recommended

    // Saturation (in-loop)
    void setSaturation(float drive, float mix){
        pSatDrive.set(drive);
        pSatMix.set(mix);
    }

    // Time modulation (external LFO writes into mod input)
    // Convention: setTimeModInput() expects [-1, +1]
    void setTimeModInput(float x)           { mTimeMod.set(x); }
    void setTimeModAmount(float seconds)    { pTimeModAmtSec.set(std::max(0.0f, seconds)); }

    // Interpolation mode (TRUNC/LINEAR/CUBIC/ALLPASS…)
    void setInterpolation(IplType t)        { mDelay.setIpolType(t); }

    // Stability controller
    // maxFb: hard cap (e.g. 0.995); limiterThreshold: loop peak guard; limiterRelease: 0.999..0.9999
    void setStability(float maxFb, float limiterThreshold, float limiterRelease){
        pMaxFeedback.set(gdsp_clampf(maxFb, 0.8f, 0.9999f));
        pLimiterThresh.set(std::max(0.001f, limiterThreshold));
        pLimiterRelease.set(gdsp_clampf(limiterRelease, 0.9f, 0.99999f));
    }

    void setSmoothingMs(float ms){
        pDelaySec.setTime(ms);
        pFeedback01.setTime(ms);
        pRT60.setTime(ms);
        pUseRT60.setTime(ms);
        pMix01.setTime(ms);
        pInGain.setTime(ms);
        pWetGain.setTime(ms);
        pDryGain.setTime(ms);
        pHPFHz.setTime(ms);
        pLPFHz.setTime(ms);
        pTonePivotHz.setTime(ms);
        pToneTiltDb.setTime(ms);
        pSatDrive.setTime(ms);
        pSatMix.setTime(ms);
        pTimeModAmtSec.setTime(ms);
        pMaxFeedback.setTime(ms);
        pLimiterThresh.setTime(ms);
        pLimiterRelease.setTime(ms);
    }

    // ---------- audio ----------
    float process(float input) override {
        // smooth params
        const float inGain   = pInGain.process();
        const float wetGain  = pWetGain.process();
        const float dryGain  = pDryGain.process();
        const float mix      = pMix01.process();

        float delaySec       = pDelaySec.process();
        const float modAmt   = pTimeModAmtSec.process();

        // time modulation (safe)
        float mod = mTimeMod.mod(); // scaled by modAmount*modScale if you choose to use those fields
        float t = delaySec + (mod * modAmt);

        // keep time in range
        const float maxT = mDelay.getMaxDelayTime();
        t = gdsp_clampf(t, 0.0f, std::max(0.0f, maxT - 1e-5f));
        mDelay.setDelay(t);

        // update loop modules
        mHPF.setCutoffHz(pHPFHz.process());
        mLPF.setCutoffHz(pLPFHz.process());

        mTone.setPivot(pTonePivotHz.process());
        mTone.setTiltDb(pToneTiltDb.process());

        mSat.setDrive(pSatDrive.process());
        mSat.setMix(pSatMix.process());

        mLimiter.setThreshold(pLimiterThresh.process());
        mLimiter.setRelease(pLimiterRelease.process());

        // compute feedback gain (raw or RT60)
        float g = 0.0f;
        const float useRT60 = pUseRT60.process(); // 1 => RT60, 0 => raw
        if(useRT60 >= 0.5f){
            const float rt60 = pRT60.process();
            g = gdsp_feedback_from_rt60(t, rt60);
        } else {
            // raw [0..1] -> clamp to safe
            float fb01 = pFeedback01.process();
            // perceptual shaping (optional): square for finer control near low values
            g = fb01 * fb01;
        }

        // stability shaping: cap feedback based on brightness & modulation
        const float maxFb = pMaxFeedback.process();

        // crude “spectral risk” heuristic:
        // - brighter LPF => lower allowed g
        // - positive tilt => lower allowed g
        // - more modulation => lower allowed g
        const float lpfHz = std::max(5.0f, pLPFHz.operator()());
        const float tilt  = pToneTiltDb.operator()(); // dB
        float bright01 = gdsp_clampf((lpfHz - 1500.0f) / (12000.0f - 1500.0f), 0.0f, 1.0f);
        float tilt01   = gdsp_clampf((tilt + 12.0f) / 24.0f, 0.0f, 1.0f);
        float mod01    = gdsp_clampf(modAmt / 0.02f, 0.0f, 1.0f); // 20ms is “a lot” for delay modulation

        float risk = 0.45f*bright01 + 0.35f*tilt01 + 0.20f*mod01;
        float cap  = maxFb * (1.0f - 0.35f * risk);

        g = std::min(g, cap);
        g = gdsp_clampf(g, 0.0f, maxFb);

        // core I/O
        float x = input * inGain;
        float d = mDelay.read();          // delayed sample

        // feedback loop: damping -> tone -> sat -> energy limiter -> gain
        float loop = d;
        loop = mHPF.process(loop);
        loop = mLPF.process(loop);
        loop = mTone.process(loop);
        loop = mSat.process(loop);
        loop = mLimiter.process(loop);
        loop *= g;

        // write back
        mDelay.write(x + loop);

        // wet/dry mix (linear; swap to equal-power if desired)
        float wet = d * wetGain;
        float dry = x * dryGain;
        return dry * (1.0f - mix) + wet * mix;
    }

    void run(const float* input, float* output, size_t n){
        for(size_t i = 0; i < n; ++i) output[i] = process(input[i]);
    }

private:
    // time engine
    Delay mDelay;

    // modulation input
    Modulator mTimeMod;

    // loop DSP blocks
    OnePoleHP mHPF;
    OnePoleLP mLPF;
    TiltTone  mTone;
    SoftClip  mSat;
    LoopEnergyLimiter mLimiter;

    // smoothed params
    ParamLinear pDelaySec       {0.25f, 15.0f};
    ParamLinear pFeedback01     {0.35f, 15.0f}; // raw mode
    ParamLinear pRT60           {3.0f,  15.0f}; // RT60 mode
    ParamLinear pUseRT60        {0.0f,  15.0f}; // 0=raw, 1=rt60

    ParamLinear pMix01          {0.35f, 15.0f};
    ParamLinear pInGain         {1.0f,  15.0f};
    ParamLinear pWetGain        {1.0f,  15.0f};
    ParamLinear pDryGain        {1.0f,  15.0f};

    ParamLinear pHPFHz          {100.0f, 15.0f};
    ParamLinear pLPFHz          {6500.0f,15.0f};

    ParamLinear pTonePivotHz    {900.0f, 15.0f};
    ParamLinear pToneTiltDb     {0.0f,   15.0f};

    ParamLinear pSatDrive       {1.5f,   15.0f};
    ParamLinear pSatMix         {0.5f,   15.0f};

    ParamLinear pTimeModAmtSec  {0.0f,   15.0f};

    // stability params
    ParamLinear pMaxFeedback    {0.995f, 15.0f};
    ParamLinear pLimiterThresh  {1.0f,   15.0f};
    ParamLinear pLimiterRelease {0.9996f,15.0f};
};
