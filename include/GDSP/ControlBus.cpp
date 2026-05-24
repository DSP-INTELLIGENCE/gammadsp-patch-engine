#include "ControlBus.hpp"
#include <algorithm>

ControlBus::ControlBus(float sr)
: mGate(sr),
  mDynamics(sr),
  mLimiter(sr)
{
    mLimiter.setCeiling(0.98f);
}

void ControlBus::reset()
{
    mGate.reset();
    mDynamics.reset();
    mLimiter.reset();

    mFrame = {};
}

float ControlBus::process(float x, const AnalysisBus& analysis)
{
    auto& F = analysis.features();
    auto& M = analysis.musical();
    auto& E = analysis.expression();

    // ---- Intelligent Gate ----
    float gated = mGate.process(x);

    // ---- Auto Dynamics ----
    float dyn = mDynamics.process(gated);

    // ---- Expressive Mix Control ----
    float expressionBoost = 1.0f + 0.5f * E.expressiveness;
    float mixed = dyn * expressionBoost;

    // ---- Safety Limiter ----
    float out = mLimiter.process(mixed);

    // ---- Telemetry for UI/Automation ----
    mFrame.masterGain = mLimiter.gain();
    mFrame.mixGain = expressionBoost;
    mFrame.compression = mDynamics.gain();
    mFrame.gate = F.active ? 1.0f : 0.0f;
    mFrame.fxSend = E.intensity;

    return out;
}
