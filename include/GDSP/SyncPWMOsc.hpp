#pragma once
#include "SuperSquareOsc.hpp"

class SyncPWMOsc : public ModGenerator {
public:
    SyncPWMOsc(float freq = 110.f, unsigned voices = 7)
    : mSlave(freq, voices), mMaster(freq) {}

    void loadSerumFile(const std::string& path) { mSlave.loadSerumFile(path); }

    void setFreq(float f) {
        baseFreq = f;
        mSlave.setFreq(f);
        mMaster.setFreq(f);
    }

    void setSyncRatio(float r) {
        mSyncRatio = r;
        mMaster.setFreq(baseFreq * r);
    }

    void setPulseWidth(float pw) { mSlave.setPulseWidth(pw); }

    float process() override {
        // advance master
        float m = mMaster.process();

        // detect wrap manually
        mMasterPhase += mMasterPhaseInc;
        if(mMasterPhase >= 1.f) {
            mMasterPhase -= 1.f;
            mSlave.reset(); // HARD SYNC
        }

        // render slave with full FM/PM/AM
        return update(mSlave);
    }

private:
    SuperSquareOsc mSlave;
    WavetableOsc   mMaster;

    float mSyncRatio = 2.f;
    float mMasterPhase = 0.f;
    float mMasterPhaseInc = 0.f;
};
