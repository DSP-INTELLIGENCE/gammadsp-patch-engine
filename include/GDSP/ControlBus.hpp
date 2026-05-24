#pragma once
#include "AnalysisBus.hpp"
#include "AnalyzerGate.hpp"
#include "EnergyDynamics.hpp"
#include "LookaheadLimiter.hpp"

struct ControlFrame {
    float masterGain;
    float mixGain;
    float fxSend;
    float compression;
    float gate;
};

class ControlBus {
public:
    ControlBus(float sampleRate);

    void reset();

    // Run after AnalysisBus per sample
    float process(float x, const AnalysisBus& analysis);

    const ControlFrame& frame() const { return mFrame; }

private:
    AnalyzerGate      mGate;
    EnergyDynamics    mDynamics;
    LookaheadLimiter  mLimiter;

    ControlFrame mFrame;
};
