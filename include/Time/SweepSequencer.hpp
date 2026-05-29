#include "GDSP_Oscillator.hpp"


class Clock {
public:
    Clock(float bpm = 120.0f) {
        setBPM(bpm);
    }

    void setBPM(float bpm) {
        mBPM = bpm;
        float beatsPerSec = bpm / 60.0f;
        mSweep.setFreq(beatsPerSec);
    }

    bool process() {
        float p = mSweep.process();
        bool tick = (p < mLastPhase);  // wrapped around
        mLastPhase = p;
        return tick;
    }

private:
    Sweep mSweep;
    float mBPM;
    float mLastPhase = 0.0f;
};

class StepSequencer {
public:
    StepSequencer(int steps = 16)
        : mSteps(steps), mIndex(0)
    {
        mPattern.resize(steps, 0.0f);
    }

    void setStep(int i, float value) {
        if (i >= 0 && i < mSteps) mPattern[i] = value;
    }

    void setSteps(int n) {
        mSteps = n;
        mPattern.resize(n);
        mIndex = 0;
    }

    bool process(bool tick) {
        if (tick) {
            mIndex = (mIndex + 1) % mSteps;
            return true;
        }
        return false;
    }

    float value() const {
        return mPattern[mIndex];
    }

    int index() const { return mIndex; }

private:
    int mSteps;
    int mIndex;
    std::vector<float> mPattern;
};

/*
Usage:
Clock clock(120.0f);
StepSequencer seq(8);

seq.setStep(0, 60);
seq.setStep(1, 62);
seq.setStep(2, 64);
seq.setStep(3, 67);
seq.setStep(4, 72);

Envelope env;
Oscillator osc;

while (rendering) {
    bool tick = clock.process();

    if (seq.process(tick)) {
        osc.setFreq(midiToFreq(seq.value()));
        env.noteOn();
    }

    float out = osc.process() * env.process();
    audioBuffer[i] = out;
}
*/