// Triode2Stage.hpp
#pragma once
#include <cmath>
#include <algorithm>

#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"
#include "GDSP_Analog.hpp"   // for TPT1Pole (or include your TPT1Pole header)

// Soft asymmetric triode-ish nonlinearity.
// k controls hardness; asym controls even harmonics; bias shifts operating point.
static inline float triodeShape(float x, float k, float asym, float bias)
{
    float v = x + bias;

    // k must be > 0
    k = std::max(0.001f, k);

    if (v >= 0.f)
    {
        // positive half: softer
        return 1.0f - std::exp(-v * k);
    }
    else
    {
        // negative half: a bit harder depending on asym
        float kk = k * (1.0f + 0.6f * std::clamp(asym, 0.f, 1.f));
        return -(1.0f - std::exp(v * kk));
    }
}

// Gentle limiter to prevent nasty blowups in extreme settings
static inline float softLimit(float x)
{
    return x / (1.f + std::fabs(x));
}

class Triode2Stage : public Function
{
public:
    Triode2Stage() = default;

    // Call this from your engine with the current sample rate
    void prepare(float sampleRate)
    {
        mSR = std::max(1.f, sampleRate);

        // coupling cap HPF after preamp (tightens low end / removes DC)
        mCouplingHP.setHPCut(mCouplingHPHz, mSR);

        reset();

        // seed modulators with defaults
        pPreDrive.set(mPreDrive);
        pPreBias.set(mPreBias);
        pPreCurve.set(mPreCurve);

        pPowDrive.set(mPowDrive);
        pPowBias.set(mPowBias);
        pPowCurve.set(mPowCurve);

        pSagAmt.set(mSagAmt);
        pSagRate.set(mSagRate);
        pNFB.set(mNFB);
        pOutGain.set(mOutGain);
        pAM.set(1.f);
    }

    // ------------------- Controls -------------------
    void setPreDrive(float v)  { mPreDrive = std::max(0.f, v); pPreDrive.set(mPreDrive); }
    void setPreBias(float v)   { mPreBias  = std::clamp(v, -0.8f, 0.8f); pPreBias.set(mPreBias); }
    void setPreCurve(float v)  { mPreCurve = std::max(0.01f, v); pPreCurve.set(mPreCurve); }

    void setPowerDrive(float v){ mPowDrive = std::max(0.f, v); pPowDrive.set(mPowDrive); }
    void setPowerBias(float v) { mPowBias  = std::clamp(v, -0.8f, 0.8f); pPowBias.set(mPowBias); }
    void setPowerCurve(float v){ mPowCurve = std::max(0.01f, v); pPowCurve.set(mPowCurve); }

    void setAM(float v)        { pAM.set(v); }

    // Interaction / “amp feel”
    void setCouplingHPHz(float hz)
    {
        mCouplingHPHz = std::clamp(hz, 1.f, 200.f);
        // update immediately if prepared
        if (mSR > 1.f) mCouplingHP.setHPCut(mCouplingHPHz, mSR);
    }

    void setSag(float v)           { mSagAmt  = std::clamp(v, 0.f, 1.f); pSagAmt.set(mSagAmt); }
    void setSagRateHz(float hz)    { mSagRate = std::clamp(hz, 0.1f, 50.f); pSagRate.set(mSagRate); }
    void setNegFeedback(float v)   { mNFB     = std::clamp(v, 0.f, 0.95f); pNFB.set(mNFB); }
    void setOutputGain(float v)    { mOutGain = std::max(0.f, v); pOutGain.set(mOutGain); }

    void reset()
    {
        mCouplingHP.reset();
        mSagLP = 0.f;
        mPowY1 = 0.f;
    }

    // ------------------- Processing -------------------
    float process(float input) override
    {
        // Pull params
        float preD = std::max(0.f,  pPreDrive.process());
        float preB = std::clamp(    pPreBias.process(),  -0.8f, 0.8f);
        float preK = std::max(0.01f,pPreCurve.process());

        float powD = std::max(0.f,  pPowDrive.process());
        float powB = std::clamp(    pPowBias.process(),  -0.8f, 0.8f);
        float powK = std::max(0.01f,pPowCurve.process());

        float sagAmt  = std::clamp(pSagAmt.process(),  0.f, 1.f);
        float sagRate = std::clamp(pSagRate.process(), 0.1f, 50.f);
        float nfb     = std::clamp(pNFB.process(),     0.f, 0.95f);

        float outGain = std::max(0.f, pOutGain.process());
        float am      = pAM.process();

        // 1) PREAMP
        float x = input * preD;
        float pre = triodeShape(x, preK, /*asym*/0.7f, preB);

        // 2) COUPLING CAP (HPF): removes DC from bias + “tightens” low end
        float coupled = mCouplingHP.processHP(pre);

        // 3) SAG / SUPPLY COMPRESSION
        // Envelope follower (rectify + one-pole LP)
        float env = std::fabs(coupled);
        float a = std::exp(-2.0f * TPT1Pole::kPi * sagRate / std::max(1.f, mSR));
        mSagLP = a * mSagLP + (1.f - a) * env;

        // sagGain decreases as envelope grows (more hit => more compression)
        float sagGain = 1.f / (1.f + sagAmt * 2.0f * mSagLP);

        // 4) POWER AMP with NEGATIVE FEEDBACK interaction
        float driveIn = coupled * sagGain * powD;
        float fbIn = driveIn - nfb * mPowY1;

        float pow = triodeShape(fbIn, powK, /*asym*/1.0f, powB);

        // store for feedback
        mPowY1 = pow;

        // 5) Safety limiter
        float y = softLimit(pow);

        return y * outGain * am;
    }

    void run(const float* in, float* out, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }

private:
    float mSR = 44100.f;

    // Preamp defaults
    float mPreDrive = 2.0f;
    float mPreBias  = 0.15f;
    float mPreCurve = 2.0f;

    // Power amp defaults
    float mPowDrive = 2.0f;
    float mPowBias  = 0.05f;
    float mPowCurve = 2.5f;

    // Interaction defaults
    float mCouplingHPHz = 15.f;
    TPT1Pole mCouplingHP;

    float mSagAmt  = 0.35f;
    float mSagRate = 6.0f;     // Hz
    float mSagLP   = 0.f;

    float mNFB     = 0.2f;     // 0..0.95
    float mPowY1   = 0.f;      // feedback memory

    float mOutGain = 1.0f;

    // Modulators
    Modulator pPreDrive;
    Modulator pPreBias;
    Modulator pPreCurve;

    Modulator pPowDrive;
    Modulator pPowBias;
    Modulator pPowCurve;

    Modulator pSagAmt;
    Modulator pSagRate;
    Modulator pNFB;
    Modulator pOutGain;
    Modulator pAM;
};
