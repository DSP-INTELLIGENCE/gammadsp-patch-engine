#pragma once
#include "Biquad.hpp"
#include "EnvFollow.hpp"

class DynamicEQBand
{
public:
    void setup(float freq, float q, float baseGainDB)
    {
        mFreq     = freq;
        mQ        = q;
        mBaseGain = baseGainDB;

        // Audio path EQ
        mEQ.setType(gam::PEAKING);
        mEQ.setFreq(freq);
        mEQ.setRes(q);

        // Detector filter
        mDetector.setType(gam::BAND_PASS);
        mDetector.setFreq(freq);
        mDetector.setRes(q);

        updateGain();
    }

    void setThreshold(float t) { mThreshold = t; }
    void setRatio(float r)     { mRatio     = r; }
    void setAttack(float a)    { mEnvelope.lag(a); }
    void setRelease(float r)   { mEnvelope.lag(r); }

    inline float process(float x)
    {
        // --- Detection path ---
        float detect = mDetector.process(x);
        float env    = mEnvelope(std::abs(detect));

        // --- Gain computer (in dB domain) ---
        float gainDB = mBaseGain;

        if (env > mThreshold)
        {
            float over = env - mThreshold;
            float reduction = over * (1.0f - 1.0f / mRatio);
            gainDB -= reduction;
        }

        // --- Apply to EQ ---
        mEQ.setLevel(dbToLinear(gainDB));
        return mEQ.process(x);
    }

private:
    Biquad    mEQ;
    Biquad    mDetector;
    EnvFollow mEnvelope;

    float mFreq     = 1000.0f;
    float mQ        = 1.0f;
    float mBaseGain = 0.0f;

    float mThreshold = 0.1f;
    float mRatio     = 2.0f;

    inline void updateGain()
    {
        mEQ.setLevel(dbToLinear(mBaseGain));
    }

    static inline float dbToLinear(float db)
    {
        return std::pow(10.0f, db * 0.05f);
    }
};
