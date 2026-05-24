// ActivityAnalyzer.hpp
#pragma once
#include <cassert>
#include <vector>
#include <memory>
#include <atomic>
#include <cmath>
#include "RMSFollower.hpp"
//#include "SilenceDetect.hpp"


class ActivityAnalyzer: public Module {
public:
    
    ActivityAnalyzer(float sr)
    : mRMS(sr, 0.05f),
    mSilence(2000),
    mThreshold(0.001f),
    mActive(false),
    mLevel(0.0f)
    {}

    void setSilenceThreshold(float t){
        mThreshold = std::max(0.000001f, t);
    }

    void setSilenceCount(unsigned samples){
        mSilence.setCount(samples);
    }

    void reset(){
        mActive = false;
        mLevel  = 0.0f;
        mSilence.reset();
    }

    void process(float x){
        float rms = mRMS.process(x);
        mLevel = rms;

        mSilence.setThreshold(mThreshold);
        bool silent = mSilence.process(rms);
        mActive = !silent;
    }

    bool isActive() const { return mActive; }
    float activityLevel() const { return mLevel; }

private:
    RMSFollower   mRMS;
    SilenceDetect mSilence;

    float mThreshold;
    bool  mActive;
    float mLevel;
};
