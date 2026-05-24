#include "AutoCompressor.hpp"
#include <algorithm>

AutoCompressor::AutoCompressor()
{
    reset();
}

void AutoCompressor::reset()
{
    mThreshold = -18.0f;
    mRatio     = 2.0f;
    mAttack    = 20.0f;
    mRelease   = 150.0f;
    mMakeup    = 3.0f;
}

void AutoCompressor::process(float energy,
                             float arousal,
                             float tension,
                             float grooveTightness)
{
    // Target values
    float tThresh = -12.0f - energy * 18.0f;              // -12 → -30
    float tRatio  = 2.0f  + tension * 6.0f;               // 2 → 8
    float tAttack = 30.0f - grooveTightness * 25.0f;      // 30 → 5 ms
    float tRel    = 300.0f - arousal * 220.0f;            // 300 → 80 ms
    float tMakeup = 2.0f + energy * 6.0f;                 // 2 → 8 dB

    // Smooth all parameters (musical inertia)
    mThreshold = 0.9f * mThreshold + 0.1f * tThresh;
    mRatio     = 0.9f * mRatio     + 0.1f * tRatio;
    mAttack    = 0.9f * mAttack    + 0.1f * tAttack;
    mRelease   = 0.9f * mRelease   + 0.1f * tRel;
    mMakeup    = 0.9f * mMakeup    + 0.1f * tMakeup;
}
