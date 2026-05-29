#pragma once
#include "AnalysisBus.hpp"
#include "ControlBus.hpp"
#include "AutoArranger.hpp"

struct IntelligenceFrame {
    AudioFeatures    features;
    MusicalState     musical;
    ExpressionState  expression;
    ControlFrame     control;
    ArrangementCommand arrangement;
};

class AudioIntelligence {
public:
    AudioIntelligence(float sampleRate);

    void reset();

    // main processing call
    float process(float x);

    const IntelligenceFrame& frame() const { return mFrame; }

private:
    AnalysisBus   mAnalysis;
    ControlBus    mControl;
    AutoArranger  mArranger;

    IntelligenceFrame mFrame;
};
