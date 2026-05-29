#pragma once
#include <cmath>
#include <algorithm>
#include "EchoMachine.hpp"
// Assumes your project provides these (as shown earlier):
//   - EchoMachine (stereo tape delay)
//   - OnePole / BlockDC / (optional) TiltFilter, etc.
//   - gam::sampleRate() available if you use Gamma
//
// This file stays strictly "control-domain + routing" for adaptive echo compression.
// The EchoMachine audio path is untouched; we only modulate feedback gain (and optionally wet gain).

// ------------------------------------------------------------
// Small utilities
// ------------------------------------------------------------
static inline float AE_dbToLin(float db){
    return std::pow(10.0f, db / 20.0f);
}
static inline float AE_linToDb(float x){
    return 20.0f * std::log10(std::max(x, 1e-20f));
}
static inline float AE_clamp(float x, float lo, float hi){
    return std::max(lo, std::min(hi, x));
}

// ------------------------------------------------------------
// Minimal AdaptiveControllerAR (domain-agnostic)
// α = 1 - exp(-1 / (τ*Fs))
// ------------------------------------------------------------
class AdaptiveControllerAR {
public:
    AdaptiveControllerAR(float attackSec = 0.01f, float releaseSec = 0.25f)
    {
        attack(attackSec);
        release(releaseSec);
        reset(0.0f);
    }

    void setSampleRate(float sr){
        mSR = std::max(1.0f, sr);
        update();
    }

    void reset(float v = 0.0f){ mValue = v; }

    void attack(float sec){
        mAttack = std::max(1e-6f, sec);
        update();
    }

    void release(float sec){
        mRelease = std::max(1e-6f, sec);
        update();
    }

    void target(float t){ mTarget = t; }

    float process(){
        const float goal  = mTarget;
        const float alpha = (goal > mValue) ? mAlphaA : mAlphaR;
        mValue += alpha * (goal - mValue);
        return mValue;
    }

    float value() const { return mValue; }

private:
    void update(){
        // α = 1 − exp(−1 / (τ·Fs))
        mAlphaA = 1.0f - std::exp(-1.0f / (mAttack  * mSR));
        mAlphaR = 1.0f - std::exp(-1.0f / (mRelease * mSR));
        mAlphaA = AE_clamp(mAlphaA, 0.0f, 1.0f);
        mAlphaR = AE_clamp(mAlphaR, 0.0f, 1.0f);
    }

    float mSR      = 48000.0f;
    float mAttack  = 0.01f;
    float mRelease = 0.25f;

    float mAlphaA  = 0.1f;
    float mAlphaR  = 0.01f;

    float mTarget  = 0.0f;
    float mValue   = 0.0f;
};

// ------------------------------------------------------------
// Minimal weighted goal blender (control-domain)
// ------------------------------------------------------------
template<int N>
class AdaptiveBlendController {
public:
    void reset(){
        for(int i=0;i<N;++i){ goals[i]=0.0f; weights[i]=0.0f; }
    }

    void goal(int i, float value, float weight){
        if(i < 0 || i >= N) return;
        goals[i]   = value;
        weights[i] = std::max(0.0f, weight);
    }

    float blended() const {
        float sum=0.0f, wsum=0.0f;
        for(int i=0;i<N;++i){
            sum  += goals[i] * weights[i];
            wsum += weights[i];
        }
        return (wsum > 0.0f) ? (sum / wsum) : 0.0f;
    }

private:
    float goals[N]{};
    float weights[N]{};
};

// ------------------------------------------------------------
// Simple envelope detector (AR) for sidechain energy in dB
// Uses absolute peak energy -> dB, smoothed with AR.
// ------------------------------------------------------------
class AEDetectorDb {
public:
    AEDetectorDb(float atkSec = 0.002f, float relSec = 0.060f)
    : mAR(atkSec, relSec)
    {}

    void setSampleRate(float sr){ mAR.setSampleRate(sr); }
    void setAttack(float sec){ mAR.attack(sec); }
    void setRelease(float sec){ mAR.release(sec); }
    void reset(float db = -120.0f){ mAR.reset(db); mLastDb = db; }

    // input is linear amplitude (already abs/energy)
    float processLin(float xLin){
        const float db = AE_linToDb(std::max(xLin, 1e-20f));
        mAR.target(db);
        mLastDb = mAR.process();
        return mLastDb;
    }

    float valueDb() const { return mLastDb; }

private:
    AdaptiveControllerAR mAR;
    float mLastDb = -120.0f;
};

// ------------------------------------------------------------
// Adaptive Echo Compressor Machine (base)
// - EchoMachine audio path stays intact.
// - We compute control GR(dB) from:
//     (A) input ducking over-threshold
//     (B) feedback safety over-threshold
//   then blend, then smooth with AdaptiveControllerAR.
// - We apply result as feedback gain trim (linear).
// - Optional: expose a "sidechain monitor" signal (control-path only).
// ------------------------------------------------------------
class AdaptiveEchoCompressorMachine {
public:
    AdaptiveEchoCompressorMachine(float maxDelaySeconds = 2.0f)
    : echo(maxDelaySeconds),
      inDetDb(0.002f, 0.060f),
      fbDetDb(0.005f, 0.120f),
      grAR(0.010f, 0.250f)
    {
        setSampleRate(48000.0f);
        setThresholdDb(-18.0f);
        setRatio(4.0f);
        setDuck(0.75f);              // 1=mostly input ducking, 0=mostly feedback safety
        setMakeupDb(0.0f);

        setGRAttack(0.010f);
        setGRRelease(0.250f);

        // defaults (pass-through to EchoMachine)
        echo.setTime(0.35f);
        echo.setFeedback(0.55f);
        echo.setMix(0.45f);
        echo.setTone(6000.0f);
        echo.setDrive(0.4f);
        echo.setWow(0.25f, 0.003f);
        echo.setFlutter(6.0f, 0.0006f);

        reset();
    }

    // ---------------- lifecycle ----------------
    void setSampleRate(float sr){
        mSR = std::max(1.0f, sr);
        inDetDb.setSampleRate(mSR);
        fbDetDb.setSampleRate(mSR);
        grAR.setSampleRate(mSR);
    }

    void reset(){
        inDetDb.reset(-120.0f);
        fbDetDb.reset(-120.0f);
        grAR.reset(0.0f);
        blend.reset();
        mGRdB = 0.0f;
        mSidechainDb = -120.0f;
        mFbSafetyDb  = -120.0f;
        mFbTrimLin   = 1.0f;
    }

    // ---------------- macro controls ----------------
    void setThresholdDb(float db){ thresholdDb = db; }
    void setRatio(float r){ ratio = std::max(1.0f, r); }

    // Duck balance: 1 = prioritize input ducking, 0 = prioritize feedback safety
    void setDuck(float d){ duck = AE_clamp(d, 0.0f, 1.0f); }

    void setMakeupDb(float db){ makeupDb = db; }

    // GR smoother
    void setGRAttack(float sec){ grAR.attack(sec); }
    void setGRRelease(float sec){ grAR.release(sec); }

    // Detector timing (separate from GR smoothing)
    void setInputDetectAR(float atkSec, float relSec){
        inDetDb.setAttack(atkSec);
        inDetDb.setRelease(relSec);
    }
    void setFeedbackDetectAR(float atkSec, float relSec){
        fbDetDb.setAttack(atkSec);
        fbDetDb.setRelease(relSec);
    }

    // ---------------- echo controls (forwarders) ----------------
    void setTime(float sec)      { echo.setTime(sec); }
    void setFeedback(float fb)   { echo.setFeedback(fb); }
    void setMix(float m)         { echo.setMix(m); }
    void setTone(float cutoffHz) { echo.setTone(cutoffHz); }
    void setDrive(float d)       { echo.setDrive(d); }
    void setWow(float rateHz, float depthSec)      { echo.setWow(rateHz, depthSec); }
    void setFlutter(float rateHz, float depthSec)  { echo.setFlutter(rateHz, depthSec); }

    // ---------------- meters / taps ----------------
    float gainReductionDb() const { return mGRdB; }         // negative or zero
    float sidechainDb() const     { return mSidechainDb; }  // input detector
    float feedbackDb() const      { return mFbSafetyDb; }   // feedback detector
    float feedbackTrimLin() const { return mFbTrimLin; }    // applied trim

    // Sidechain monitor (control-domain): returns a normalized “what the detector sees”
    // Not an audio tap. This is a convenience signal for UI / debugging.
    float sidechainMonitor01() const {
        // map [-60..0] dB to [0..1]
        return AE_clamp((mSidechainDb + 60.0f) / 60.0f, 0.0f, 1.0f);
    }

    // ---------------- DSP ----------------
    inline StereoSample processStereo(float inL, float inR)
    {
        // 1) detectors
        const float inEnergy = 0.5f * (std::fabs(inL) + std::fabs(inR));
        const float fbEnergy = 0.5f * (std::fabs(mLastWetL) + std::fabs(mLastWetR));

        mSidechainDb = inDetDb.processLin(inEnergy);
        mFbSafetyDb  = fbDetDb.processLin(fbEnergy);

        // 2) build over-threshold goals (dB over)
        const float overIn = std::max(0.0f, mSidechainDb - thresholdDb);
        const float overFb = std::max(0.0f, mFbSafetyDb  - thresholdDb);

        // 3) explicit blend of intents
        // goal0: ducking from input
        // goal1: safety from feedback
        blend.goal(0, overIn, duck);
        blend.goal(1, overFb, 1.0f - duck);

        const float overDb = blend.blended();

        // 4) compression law in dB (negative GR)
        float targetGRdB = 0.0f;
        if(overDb > 0.0f){
            targetGRdB = -overDb * (1.0f - 1.0f / ratio);
        }

        // 5) asymmetric smoothing on GR
        grAR.target(targetGRdB);
        mGRdB = grAR.process();

        // 6) apply to feedback trim (and optional makeup)
        const float makeupLin = AE_dbToLin(makeupDb);
        mFbTrimLin = AE_dbToLin(mGRdB) * makeupLin;

        // 7) run echo (audio)
        StereoSample out = echo.processStereo(inL, inR);

        // 8) store wet estimates (for feedback detector)
        // If EchoMachine doesn’t expose wet taps, just approximate with output - input * dryG,
        // but your EchoMachine already tracks internal dl/dr; if you add getters, wire them here.
        // For now, approximate wet component from output vs input (stable enough for safety control):
        mLastWetL = out.L - inL;
        mLastWetR = out.R - inR;

        // 9) apply trim by scaling feedback inside EchoMachine?
        // We do NOT edit EchoMachine internals here.
        // The intended integration is: EchoMachine uses an external feedback scale factor.
        // If you already have such a hook, call it here. Otherwise, you can route by:
        //    echo.setFeedback( baseFeedback * mFbTrimLin )  (control-only)
        //
        // We keep it control-domain and safe:
        echo.setFeedback( AE_clamp(baseFeedback * mFbTrimLin, 0.0f, 0.95f) );

        return out;
    }

    inline StereoSample process(float x){
        return processStereo(x, x);
    }

    // If you want feedback to be modulated without stomping user feedback continuously,
    // store their “base” setting separately:
    void setBaseFeedback(float fb){
        baseFeedback = AE_clamp(fb, 0.0f, 0.95f);
        echo.setFeedback(baseFeedback);
    }

    EchoMachine& getEcho() { return echo; }

private:
    float mSR = 48000.0f;

    // Core
    EchoMachine echo;

    // Detectors (dB)
    AEDetectorDb inDetDb;
    AEDetectorDb fbDetDb;

    // Intent blend + GR smoothing
    AdaptiveBlendController<2> blend;
    AdaptiveControllerAR       grAR;

    // Params
    float thresholdDb  = -18.0f;
    float ratio        = 4.0f;
    float duck         = 0.75f;
    float makeupDb     = 0.0f;

    // User base feedback (so adaptive trim doesn’t erase user intent)
    float baseFeedback = 0.55f;

    // State / meters
    float mGRdB        = 0.0f;
    float mSidechainDb = -120.0f;
    float mFbSafetyDb  = -120.0f;
    float mFbTrimLin   = 1.0f;

    float mLastWetL = 0.0f;
    float mLastWetR = 0.0f;
};

// ------------------------------------------------------------
// “Machines” = curated configurations (no extra features)
// These are thin wrappers that just set defaults.
// ------------------------------------------------------------

// 1) Classic Ducking Echo: strong input duck, gentle safety
class DuckingEchoMachine : public AdaptiveEchoCompressorMachine {
public:
    DuckingEchoMachine(float maxDelaySeconds = 2.0f)
    : AdaptiveEchoCompressorMachine(maxDelaySeconds)
    {
        setThresholdDb(-20.0f);
        setRatio(6.0f);
        setDuck(0.90f);
        setGRAttack(0.008f);
        setGRRelease(0.280f);
        setMakeupDb(0.0f);

        setTime(0.38f);
        setMix(0.42f);
        setTone(5500.0f);
        setDrive(0.35f);
        setBaseFeedback(0.58f);
    }
};

// 2) Feedback-Safe Tape Echo: prioritizes runaway control
class FeedbackSafeEchoMachine : public AdaptiveEchoCompressorMachine {
public:
    FeedbackSafeEchoMachine(float maxDelaySeconds = 2.0f)
    : AdaptiveEchoCompressorMachine(maxDelaySeconds)
    {
        setThresholdDb(-24.0f);
        setRatio(10.0f);
        setDuck(0.25f);            // mostly safety
        setGRAttack(0.012f);
        setGRRelease(0.420f);
        setMakeupDb(0.0f);

        setTime(0.45f);
        setMix(0.40f);
        setTone(4800.0f);
        setDrive(0.45f);
        setBaseFeedback(0.72f);
    }
};

// 3) Pumping Rhythmic Echo: fast attack + shorter release, more audible GR movement
class PumpingEchoMachine : public AdaptiveEchoCompressorMachine {
public:
    PumpingEchoMachine(float maxDelaySeconds = 2.0f)
    : AdaptiveEchoCompressorMachine(maxDelaySeconds)
    {
        setThresholdDb(-16.0f);
        setRatio(8.0f);
        setDuck(0.80f);
        setGRAttack(0.004f);
        setGRRelease(0.120f);
        setMakeupDb(0.0f);

        setTime(0.30f);
        setMix(0.50f);
        setTone(7000.0f);
        setDrive(0.25f);
        setBaseFeedback(0.60f);
    }
};
