#include "AutoGate.hpp"
#include <algorithm>

AutoGate::AutoGate()
{
    reset();
}

void AutoGate::reset()
{
    mThreshold = -40.0f;
    mAttack    = 5.0f;
    mRelease   = 150.0f;
    mRange     = -20.0f;
}

void AutoGate::process(float activity,
                       float noisiness,
                       float energy,
                       float tension)
{
    // Target calculations
    float tThresh = -55.0f + noisiness * 30.0f + energy * 10.0f; // -55 → -15
    float tAttack = 2.0f  + (1.0f - activity) * 10.0f;          // 2 → 12 ms
    float tRel    = 80.0f + tension * 250.0f;                  // 80 → 330 ms
    float tRange  = -10.0f - noisiness * 20.0f;                // -10 → -30 dB

    // Smooth everything
    mThreshold = 0.9f * mThreshold + 0.1f * tThresh;
    mAttack    = 0.9f * mAttack    + 0.1f * tAttack;
    mRelease   = 0.9f * mRelease   + 0.1f * tRel;
    mRange     = 0.9f * mRange     + 0.1f * tRange;
}
