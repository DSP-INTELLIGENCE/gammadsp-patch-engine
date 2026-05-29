// OnsetTrigger.hpp
#pragma once
#include "OnsetDetector.hpp"
#include "TriggerSynth.hpp"

class OnsetTrigger {
public:
    
    OnsetTrigger(float sr)
    : mDetector(sr),
    mSynth(sr),
    mTriggered(false),
    mStrength(0.0f)
    {
        mSynth.setMode(TriggerSynth::Gate);
        mSynth.setPulse(0.01f);   // 10 ms default
        mSynth.setHold(0.02f);    // 20 ms default
    }

    void setSensitivity(float s){
        mDetector.setSensitivity(s);
    }

    void setThreshold(float t){
        mDetector.setThreshold(t);
    }

    void setRefractory(float seconds){
        mDetector.setRefractory(seconds);
    }

    void setPulse(float seconds){
        mSynth.setPulse(seconds);
    }

    void setHold(float seconds){
        mSynth.setHold(seconds);
    }

    void setMode(TriggerSynth::Mode m){
        mSynth.setMode(m);
    }

    void setVelocityScale(float v){
        mSynth.setVelocityScale(v);
    }

    void reset(){
        mDetector.reset();
        mSynth.reset();
        mTriggered = false;
        mStrength = 0.0f;
    }

    float process(float x)
    {
        mTriggered = mDetector.process(x);
        mStrength  = mDetector.strength();

        return mSynth.process(mTriggered, mStrength);
    }

    // Feature access
    bool  triggered() const { return mTriggered; }
    float strength()  const { return mStrength; }

private:
    OnsetDetector mDetector;
    TriggerSynth  mSynth;

    bool  mTriggered;
    float mStrength;
};
