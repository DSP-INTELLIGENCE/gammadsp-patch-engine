#pragma once
#define INC_ANALYSIS 1
#include "GammaDSP.hpp"
#include "RMSFollower.hpp"

class AnalyzerGate {
public:
    AnalyzerGate(float sampleRate);
    : mRMS(sr, 0.05f),
    mSilence(2000),
    mGate(0.2f, 15.0f),
    mThreshold(0.001f),
    mActive(false),
    mLevel(0.0f),
    mFloor(0.0f),
    mHoldSamples(0),
    mHoldLeft(0),
    mSampleRate(sr)
    {}

    void setSilenceThreshold(float t){
        mThreshold = std::max(0.000001f, t);
    }

    void setSilenceCount(unsigned samples){
        mSilence.setCount(samples);
    }

    void setOpenThreshold(float t){
        mGate.setThreshold(t);
    }

    void setSmoothFreq(float f){
        mGate.setSmoothFreq(f);
    }

    void setHold(float seconds){
        mHoldSamples = (unsigned)(seconds * mSampleRate);
    }

    void setFloor(float g){
        mFloor = std::clamp(g, 0.0f, 1.0f);
    }

    void reset(){
        mActive = false;
        mLevel = 0.0f;
        mHoldLeft = 0;
        mSilence.reset();
        mGate.reset(0.0f);
    }

    float process(float x){
        // ---- Analyzer ----
        float rms = mRMS.process(x);
        mLevel = rms;

        bool silent = mSilence.process(rms, mThreshold);
        mActive = !silent;

        // ---- Synth ----
        float g = mGate.process(mActive ? 1.0f : 0.0f, 1.0f, mFloor);

        if(mActive){
            mHoldLeft = mHoldSamples;
        }
        else if(mHoldLeft > 0){
            --mHoldLeft;
            g = 1.0f;
        }

        return x * g;
    }

    // Feature access
    bool  isActive() const { return mActive; }
    float activityLevel() const { return mLevel; }

private:
    RMSFollower   mRMS;
    SilenceDetect mSilence;
    Threshold     mGate;

    float mThreshold;
    bool  mActive;
    float mLevel;

    float mFloor;
    unsigned mHoldSamples;
    unsigned mHoldLeft;
    float mSampleRate;
};
