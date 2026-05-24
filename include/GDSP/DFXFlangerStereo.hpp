#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>

#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif

#include "Delay.hpp"   // <-- use your wrapper, not gam::Delay directly

struct StereoSample { float L = 0.f; float R = 0.f; };

static inline float clampf(float x, float a, float b){ return std::min(std::max(x,a),b); }

static inline float softsat_tanh(float x, float drive){
    if(drive <= 0.f) return x;
    float k = 1.f + 8.f * drive;
    return std::tanh(k * x) / std::tanh(k);
}

static inline float onepole(float y, float x, float g){ return y + g * (x - y); }

struct DriftNoise {
    uint32_t s = 0x12345678u;
    float y = 0.f;
    float g = 0.001f;

    void setRateHz(float hz, float sr){
        float x = 2.f * float(M_PI) * hz / sr;
        g = 1.f - std::exp(-x);
        g = clampf(g, 0.000001f, 0.5f);
    }

    inline float next(){
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        float n = (float(int32_t(s)) / 2147483648.0f); // ~[-1,1)
        y = onepole(y, n, g);
        return y;
    }
};

class DFXStereoFlanger
{
public:
    DFXFlangerStereo()
    : mDL(0.02f, 0.001f),
      mDR(0.02f, 0.0012f)
    {
        mDL.setIpolType(gam::ipl::Type::CUBIC);
        mDR.setIpolType(gam::ipl::Type::CUBIC);

        setRate(0.15f);
        setDepthMs(0.6f);
        setBaseMs(0.8f);
        setFeedback(0.65f);
        setMix(0.5f);
        setStereoWidth(0.8f);
        setDrive(0.35f);
        setWowFlutter(0.15f, 0.6f, 0.08f);
        setTimeSmoothing(0.001f); // 1ms default

        mDLs = mBase;
        mDRs = mBase * 1.01f;
        mDL.setDelay(mDLs);
        mDR.setDelay(mDRs);
    }

    // --- Parameters ---
    void setRate(float hz)       { mRate = std::max(0.f, hz); }
    void setBaseMs(float ms)     { mBase  = std::max(0.01f, ms) * 0.001f; }
    void setDepthMs(float ms)    { mDepth = std::max(0.f, ms) * 0.001f; }
    void setFeedback(float fb)   { mFeedback = clampf(fb, -0.99f, 0.99f); }
    void setMix(float mix)       { mMix = clampf(mix, 0.f, 1.f); }
    void setDrive(float d)       { mDrive = std::max(0.f, d); }
    void setStereoWidth(float w) { mWidth = clampf(w, 0.f, 1.f); }

    // tauSeconds = 0 disables smoothing
    void setTimeSmoothing(float tauSeconds) { mTau = std::max(0.f, tauSeconds); }

    void setWowFlutter(float wowHz, float flutterHz, float amount){
        mWowHz = std::max(0.f, wowHz);
        mFlutterHz = std::max(0.f, flutterHz);
        mWFAmount = clampf(amount, 0.f, 1.f);

        float sr = (float)gam::sampleRate();
        mDrift.setRateHz(0.12f, sr);
    }

    inline StereoSample process(float inL, float inR)
    {
        const float sr = (float)gam::sampleRate();

        // Compute both L/R LFOs from the SAME phase, then advance once
        float phaseNow = mPhase;

        // main LFO shapes
        float sL = std::sinf(phaseNow);
        float sR = std::sinf(phaseNow + float(M_PI_2));

        auto shape = [](float s){
            float tri = (2.f / float(M_PI)) * std::asinf(clampf(s, -1.f, 1.f));
            return 0.55f * s + 0.45f * tri;
        };

        float lfoL = shape(sL);
        float lfoR = shape(sR);

        mPhase = phaseNow + 2.f * float(M_PI) * (mRate / sr);
        if (mPhase > 2.f * float(M_PI)) mPhase -= 2.f * float(M_PI);

        // wow/flutter + drift
        mWowPhase  += 2.f * float(M_PI) * (mWowHz / sr);
        mFlutPhase += 2.f * float(M_PI) * (mFlutterHz / sr);
        if (mWowPhase > 2.f * float(M_PI))  mWowPhase  -= 2.f * float(M_PI);
        if (mFlutPhase > 2.f * float(M_PI)) mFlutPhase -= 2.f * float(M_PI);

        float wow   = std::sinf(mWowPhase);
        float flut  = std::sinf(mFlutPhase) * std::sinf(0.5f * mFlutPhase + 1.2f);
        float drift = mDrift.next();

        float wf = (0.65f * wow + 0.25f * flut + 0.10f * drift) * (0.15f * mWFAmount);

        // stereo decorrelation: slight mismatch
        float baseL  = mBase;
        float baseR  = mBase * (1.f + 0.01f * mWidth);
        float depthL = mDepth;
        float depthR = mDepth * (1.f + 0.015f * mWidth);

        float dL = (baseL + depthL * lfoL)  * (1.f + wf);
        float dR = (baseR + depthR * lfoR)  * (1.f - 0.9f * wf);

        dL = clampf(dL, 0.00005f, 0.019f);
        dR = clampf(dR, 0.00005f, 0.019f);

        // Optional time smoothing (helps flanger feel)
        if (mTau > 0.f) {
            float g = 1.f - std::exp(-1.f / (mTau * sr));
            mDLs = onepole(mDLs, dL, g);
            mDRs = onepole(mDRs, dR, g);
        } else {
            mDLs = dL;
            mDRs = dR;
        }

        mDL.setDelay(mDLs);
        mDR.setDelay(mDRs);

        // Read delayed (previous) samples
        float wetL = mDL.read();
        float wetR = mDR.read();

        // Feedback with soft saturation + small cross-couple
        float cross = 0.12f * mWidth;

        float fbL = softsat_tanh(wetL + cross * wetR, mDrive);
        float fbR = softsat_tanh(wetR + cross * wetL, mDrive);

        mDL.write(inL + mFeedback * fbL);
        mDR.write(inR + mFeedback * fbR);

        // Equal-power mix per channel
        float theta = mMix * float(M_PI_2);
        float dryG  = std::cos(theta);
        float wetG  = std::sin(theta);

        StereoSample out;
        out.L = inL * dryG + wetL * wetG;
        out.R = inR * dryG + wetR * wetG;
        return out;
    }

private:
    // Use your wrapper Delay class
    Delay mDL, mDR;

    float mPhase     = 0.f;
    float mWowPhase  = 0.f;
    float mFlutPhase = 0.f;

    float mRate     = 0.15f;
    float mBase     = 0.0008f;
    float mDepth    = 0.0006f;
    float mFeedback = 0.65f;
    float mMix      = 0.5f;
    float mWidth    = 0.8f;
    float mDrive    = 0.35f;

    float mWowHz     = 0.15f;
    float mFlutterHz = 0.6f;
    float mWFAmount  = 0.08f;
    DriftNoise mDrift;

    float mTau = 0.001f; // seconds, 0 disables time smoothing
    float mDLs = 0.001f;
    float mDRs = 0.0012f;
};
