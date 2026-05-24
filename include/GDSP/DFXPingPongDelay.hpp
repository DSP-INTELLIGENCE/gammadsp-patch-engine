// PingPongDelay.hpp
#pragma once
#include <cmath>
#include <algorithm>
#include "Delay.hpp"
#include "Parameters.hpp"

inline float gdsp_clampf(float x, float lo, float hi){
    return std::max(lo, std::min(hi, x));
}

// --------- lightweight loop modules (same as before) ----------
struct OnePoleLP {
    float a = 0.0f;
    float y = 0.0f;
    void setCutoffHz(float hz){
        float sr = (float)gam::sampleRate();
        hz = gdsp_clampf(hz, 5.0f, 0.49f * sr);
        float x = hz / (0.5f * sr);
        a = gdsp_clampf(x, 0.0005f, 0.9995f);
    }
    float process(float x){ y += a * (x - y); return y; }
};

struct OnePoleHP {
    OnePoleLP lp;
    void setCutoffHz(float hz){ lp.setCutoffHz(hz); }
    float process(float x){ return x - lp.process(x); }
};

struct TiltTone {
    OnePoleLP lp;
    float pivotHz = 900.0f;
    float tiltDb  = 0.0f; // -12..+12 recommended
    void setPivot(float hz){ pivotHz = hz; lp.setCutoffHz(pivotHz); }
    void setTiltDb(float db){ tiltDb = db; }
    float process(float x){
        float lo = lp.process(x);
        float hi = x - lo;
        float db = gdsp_clampf(tiltDb, -12.0f, 12.0f);
        float gLo = std::pow(10.0f, (-db) / 20.0f);
        float gHi = std::pow(10.0f, ( db) / 20.0f);
        return lo * gLo + hi * gHi;
    }
};

struct SoftClip {
    float drive = 1.0f;
    float mix   = 1.0f;
    void setDrive(float d){ drive = std::max(0.0f, d); }
    void setMix(float m){ mix = gdsp_clampf(m, 0.0f, 1.0f); }
    float process(float x){
        float in = x * drive;
        float y  = in / (1.0f + std::abs(in));   // stable loop clip
        return in + (y - in) * mix;
    }
};

struct LoopEnergyLimiter {
    float env = 0.0f;
    float release   = 0.9996f;
    float threshold = 1.0f;
    void setRelease(float r){ release = gdsp_clampf(r, 0.9f, 0.99999f); }
    void setThreshold(float t){ threshold = std::max(0.0001f, t); }
    float process(float x){
        float e = std::abs(x);
        env = std::max(e, env * release);
        if(env > threshold) x *= (threshold / env);
        return x;
    }
};

inline float gdsp_feedback_from_rt60(float delaySec, float rt60Sec){
    if(rt60Sec <= 0.001f) return 0.0f;
    return std::pow(10.0f, -3.0f * delaySec / rt60Sec);
}

// =============================================================
// PingPongDelay: two delays + cross-feedback
// =============================================================
class DFXPingPongDelay {
public:
    DFXPingPongDelay(float maxDelaySeconds = 2.0f,
                  float initDelaySeconds = 0.35f)
    : dL(maxDelaySeconds, initDelaySeconds)
    , dR(maxDelaySeconds, initDelaySeconds * 0.75f)
    {
        dL.setIpolType(ALLPASS);
        dR.setIpolType(ALLPASS);
        setSmoothingMs(15.0f);

        // musical defaults
        setDelayTime(initDelaySeconds);
        setOffset(0.0f);                 // L/R same time
        setMix01(0.35f);
        setInputGain(1.0f);
        setWetGain(1.0f);
        setDryGain(1.0f);
        setCrossFeedback01(1.0f);        // classic ping-pong = full cross
        setFeedback01(0.35f);
        setUseRT60(false);
        setDecayRT60(4.0f);

        setDamping(100.0f, 6500.0f);
        setTonePivot(900.0f);
        setToneTiltDb(0.0f);
        setSaturation(1.5f, 0.5f);
        setStability(0.995f, 1.0f, 0.9996f);

        setTimeModAmount(0.0f);
    }

    // ----------------- Parameters -----------------
    void setInterpolation(IplType t){ dL.setIpolType(t); dR.setIpolType(t); }

    void setDelayTime(float seconds){ pDelaySec.set(std::max(0.0f, seconds)); }

    // Offset L/R delay time: [-1..1] maps to +/- offsetSeconds
    void setOffset(float offset01){ pOffset01.set(gdsp_clampf(offset01, -1.f, 1.f)); }
    void setOffsetSeconds(float sec){ pOffsetSec.set(std::max(0.0f, sec)); }

    // Feedback control
    void setFeedback01(float fb01){ pFeedback01.set(gdsp_clampf(fb01, 0.f, 1.f)); }

    // RT60 mode (recommended for consistent decay across times)
    void setUseRT60(bool use){ pUseRT60.set(use ? 1.0f : 0.0f); }
    void setDecayRT60(float seconds){ pRT60.set(std::max(0.001f, seconds)); }

    // Ping-pong amount: 0 = dual mono, 1 = full cross
    void setCrossFeedback01(float x){ pCross01.set(gdsp_clampf(x, 0.f, 1.f)); }

    // Mix/gains
    void setMix01(float mix01){ pMix01.set(gdsp_clampf(mix01, 0.f, 1.f)); }
    void setInputGain(float g){ pInGain.set(g); }
    void setWetGain(float g){ pWetGain.set(g); }
    void setDryGain(float g){ pDryGain.set(g); }

    // Damping (in-loop)
    void setDamping(float hpfHz, float lpfHz){ pHPFHz.set(hpfHz); pLPFHz.set(lpfHz); }

    // Tone (in-loop)
    void setTonePivot(float hz){ pTonePivotHz.set(hz); }
    void setToneTiltDb(float db){ pToneTiltDb.set(db); }

    // Saturation (in-loop)
    void setSaturation(float drive, float mix){ pSatDrive.set(drive); pSatMix.set(mix); }

    // Stability controller
    void setStability(float maxFb, float limiterThreshold, float limiterRelease){
        pMaxFeedback.set(gdsp_clampf(maxFb, 0.8f, 0.9999f));
        pLimiterThresh.set(std::max(0.001f, limiterThreshold));
        pLimiterRelease.set(gdsp_clampf(limiterRelease, 0.9f, 0.99999f));
    }

    // Time modulation
    // setTimeModInput expects [-1..+1]
    void setTimeModInput(float x){ mTimeMod.set(x); }
    void setTimeModAmount(float seconds){ pTimeModAmtSec.set(std::max(0.0f, seconds)); }

    void setSmoothingMs(float ms){
        pDelaySec.setTime(ms);
        pOffset01.setTime(ms);
        pOffsetSec.setTime(ms);
        pFeedback01.setTime(ms);
        pUseRT60.setTime(ms);
        pRT60.setTime(ms);
        pCross01.setTime(ms);
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

    // ----------------- Audio -----------------
    // Stereo processing
    inline void process(float inL, float inR, float& outL, float& outR)
    {
        // smooth shared params
        const float inGain  = pInGain.process();
        const float wetGain = pWetGain.process();
        const float dryGain = pDryGain.process();
        const float mix     = pMix01.process();

        const float baseT   = pDelaySec.process();
        const float off01   = pOffset01.process();
        const float offSec  = pOffsetSec.process();
        const float modAmt  = pTimeModAmtSec.process();
        const float modIn   = mTimeMod.mod(); // [-1..1] scaled externally if desired

        // L/R times (with offset + modulation)
        float tL = baseT + off01 * offSec + modIn * modAmt;
        float tR = baseT - off01 * offSec + modIn * modAmt;

        const float maxTL = std::max(0.0f, dL.getMaxDelayTime() - 1e-5f);
        const float maxTR = std::max(0.0f, dR.getMaxDelayTime() - 1e-5f);
        tL = gdsp_clampf(tL, 0.0f, maxTL);
        tR = gdsp_clampf(tR, 0.0f, maxTR);

        dL.setDelay(tL);
        dR.setDelay(tR);

        // update loop processors (same settings for both channels; easy to split later)
        hpfL.setCutoffHz(pHPFHz.process());  hpfR.setCutoffHz(pHPFHz.operator()());
        lpfL.setCutoffHz(pLPFHz.process());  lpfR.setCutoffHz(pLPFHz.operator()());

        toneL.setPivot(pTonePivotHz.process()); toneR.setPivot(pTonePivotHz.operator()());
        toneL.setTiltDb(pToneTiltDb.process()); toneR.setTiltDb(pToneTiltDb.operator()());

        satL.setDrive(pSatDrive.process()); satR.setDrive(pSatDrive.operator()());
        satL.setMix(pSatMix.process());     satR.setMix(pSatMix.operator()());

        limL.setThreshold(pLimiterThresh.process()); limR.setThreshold(pLimiterThresh.operator()());
        limL.setRelease(pLimiterRelease.process());  limR.setRelease(pLimiterRelease.operator()());

        // compute global feedback gain
        float g = 0.0f;
        if(pUseRT60.process() >= 0.5f){
            g = gdsp_feedback_from_rt60(0.5f * (tL + tR), pRT60.process());
        } else {
            float fb01 = pFeedback01.process();
            g = fb01 * fb01; // perceptual shaping
        }

        // stability cap (simple heuristic; you can refine)
        const float maxFb = pMaxFeedback.process();
        float cap = maxFb;

        // modulation adds risk -> reduce cap
        float mod01 = gdsp_clampf(modAmt / 0.02f, 0.0f, 1.0f);
        cap *= (1.0f - 0.15f * mod01);

        g = gdsp_clampf(std::min(g, cap), 0.0f, maxFb);

        // cross feedback amount
        const float cross = pCross01.process(); // 0=dual mono, 1=full ping-pong

        // input
        float xL = inL * inGain;
        float xR = inR * inGain;

        // read taps
        float dOutL = dL.read();
        float dOutR = dR.read();

        // build loop signals per side (process each side's delayed output)
        float loopL = dOutL;
        loopL = hpfL.process(loopL);
        loopL = lpfL.process(loopL);
        loopL = toneL.process(loopL);
        loopL = satL.process(loopL);
        loopL = limL.process(loopL);

        float loopR = dOutR;
        loopR = hpfR.process(loopR);
        loopR = lpfR.process(loopR);
        loopR = toneR.process(loopR);
        loopR = satR.process(loopR);
        loopR = limR.process(loopR);

        // cross-mix feedback: each write gets a blend of self + other
        // selfAmt = 1-cross, crossAmt = cross
        float fbToL = ((1.0f - cross) * loopL + cross * loopR) * g;
        float fbToR = ((1.0f - cross) * loopR + cross * loopL) * g;

        // write back
        dL.write(xL + fbToL);
        dR.write(xR + fbToR);

        // output mix
        float wetL = dOutL * wetGain;
        float wetR = dOutR * wetGain;
        float dryL = xL * dryGain;
        float dryR = xR * dryGain;

        outL = dryL * (1.0f - mix) + wetL * mix;
        outR = dryR * (1.0f - mix) + wetR * mix;
    }

    void run(const float* inL, const float* inR, float* outL, float* outR, size_t n){
        for(size_t i = 0; i < n; ++i){
            float l, r;
            process(inL[i], inR[i], l, r);
            outL[i] = l;
            outR[i] = r;
        }
    }

private:
    // delay lines
    Delay dL, dR;

    // modulation input
    Modulator mTimeMod;

    // per-channel loop processors
    OnePoleHP hpfL, hpfR;
    OnePoleLP lpfL, lpfR;
    TiltTone  toneL, toneR;
    SoftClip  satL, satR;
    LoopEnergyLimiter limL, limR;

    // smoothed params
    ParamLinear pDelaySec      {0.35f, 15.0f};
    ParamLinear pOffset01      {0.0f,  15.0f};  // [-1..1]
    ParamLinear pOffsetSec     {0.0f,  15.0f};  // seconds

    ParamLinear pFeedback01    {0.35f, 15.0f};  // raw feedback
    ParamLinear pUseRT60       {0.0f,  15.0f};  // 0=raw, 1=rt60
    ParamLinear pRT60          {4.0f,  15.0f};

    ParamLinear pCross01       {1.0f,  15.0f};

    ParamLinear pMix01         {0.35f, 15.0f};
    ParamLinear pInGain        {1.0f,  15.0f};
    ParamLinear pWetGain       {1.0f,  15.0f};
    ParamLinear pDryGain       {1.0f,  15.0f};

    ParamLinear pHPFHz         {100.0f, 15.0f};
    ParamLinear pLPFHz         {6500.0f,15.0f};

    ParamLinear pTonePivotHz   {900.0f, 15.0f};
    ParamLinear pToneTiltDb    {0.0f,   15.0f};

    ParamLinear pSatDrive      {1.5f,   15.0f};
    ParamLinear pSatMix        {0.5f,   15.0f};

    ParamLinear pTimeModAmtSec {0.0f,   15.0f};

    ParamLinear pMaxFeedback   {0.995f, 15.0f};
    ParamLinear pLimiterThresh {1.0f,   15.0f};
    ParamLinear pLimiterRelease{0.9996f,15.0f};
};
