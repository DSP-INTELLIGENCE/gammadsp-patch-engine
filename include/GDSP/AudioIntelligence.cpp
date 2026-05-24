#include "AudioIntelligence.hpp"

AudioIntelligence::AudioIntelligence(float sr)
: mAnalysis(sr),
  mControl(sr),
  mArranger(sr)
{}

void AudioIntelligence::reset(){
    mAnalysis.reset();
    mControl.reset();
    mArranger.reset();
}

float AudioIntelligence::process(float x)
{
    // ---- Analysis ----
    float y = mAnalysis.process(x);

    // ---- Control ----
    y = mControl.process(y, mAnalysis);

    // ---- Arrangement ----
    const auto& cmd = mArranger.process(mAnalysis);

    // ---- Collect frame for UI / scripting / engine ----
    mFrame.features   = mAnalysis.features();
    mFrame.musical    = mAnalysis.musical();
    mFrame.expression = mAnalysis.expression();
    mFrame.control    = mControl.frame();
    mFrame.arrangement= cmd;

    return y;
}
