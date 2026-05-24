// GateSynth.hpp
#pragma once
#include "Threshold.hpp"

class GateSynth {
public:
    
    GateSynth(float sr)
    : mGate(0.2f, 15.0f),
    mFloor(0.0f),
    mHoldSamples(0),
    mHoldLeft(0),
    mSampleRate(sr)
    {}

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
        mHoldLeft = 0;
        mGate.reset(0.0f);
    }

    float process(float x, bool active){
        float g = mGate.process(active ? 1.0f : 0.0f, 1.0f, mFloor);

        if(active){
            mHoldLeft = mHoldSamples;
        }
        else if(mHoldLeft > 0){
            --mHoldLeft;
            g = 1.0f;
        }

        return x * g;
    }

private:
    Threshold mGate;

    float mFloor;
    unsigned mHoldSamples;
    unsigned mHoldLeft;
    float mSampleRate;
};
