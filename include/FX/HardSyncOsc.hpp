#pragma once
#include "WavetableOsc.hpp"

class HardSyncOsc : public ModGenerator {
public:
    HardSyncOsc(float freq = 440.f)
    : mMaster(freq), mSlave(freq) {}

    void setFreq(float f) {
        baseFreq = f;
        mMaster.setFreq(f);
    }

    void setSyncRatio(float r) {
        mSyncRatio = r;
        mSlave.setFreq(baseFreq * r);
    }

    void setPosition(float p) { mSlave.setPosition(p); }

    float process() override {
        // master advances
        float m = mMaster.process();

        // detect wrap
        float phase = mMasterPhase;
        mMasterPhase += mMasterPhaseInc;
        if(mMasterPhase >= 1.f) {
            mMasterPhase -= 1.f;
            mSlave.reset();
        }

        return update(mSlave);
    }

private:
    WavetableOsc mMaster;
    WavetableOsc mSlave;

    float mSyncRatio = 2.f;
    float mMasterPhase = 0.f;
    float mMasterPhaseInc = 0.f;
};
