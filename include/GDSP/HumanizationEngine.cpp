#include "HumanizationEngine.hpp"
#include <algorithm>
#include <cstdlib>

static float frand()
{
    return (float)std::rand() / (float)RAND_MAX;
}

HumanizationEngine::HumanizationEngine(float sr)
: mSampleRate(sr),
  mTimingOffset(0),
  mVelocityScale(1),
  mArticulation(0.5f)
{}

void HumanizationEngine::reset()
{
    mTimingOffset = 0.0f;
    mVelocityScale = 1.0f;
    mArticulation = 0.5f;
}

void HumanizationEngine::process(float expression,
                                 float grooveTightness,
                                 float grooveSwing,
                                 float arousal,
                                 float movement)
{
    // Timing: tighter groove = smaller deviation
    float maxOffset = (1.0f - grooveTightness) * 0.02f; // up to 20ms

    float feelBias = (grooveSwing - 0.5f) * 0.01f;

    mTimingOffset = (frand() * 2.0f - 1.0f) * maxOffset + feelBias;

    // Velocity: expressive & emotional scaling
    float velTarget = 0.7f + expression * 0.6f + arousal * 0.3f;
    mVelocityScale = 0.9f * mVelocityScale + 0.1f * velTarget;

    // Articulation: movement controls note length / envelope
    float artTarget = std::clamp(0.3f + movement * 0.7f, 0.0f, 1.0f);
    mArticulation = 0.9f * mArticulation + 0.1f * artTarget;
}
