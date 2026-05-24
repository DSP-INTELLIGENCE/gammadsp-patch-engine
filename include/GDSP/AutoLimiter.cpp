#include "AutoLimiter.hpp"
#include <algorithm>

AutoLimiter::AutoLimiter()
{
    reset();
}

void AutoLimiter::reset()
{
    mCeiling   = -0.8f;
    mLookahead = 3.0f;
    mRelease   = 100.0f;
}

void AutoLimiter::process(float energy, float arousal)
{
    float tCeiling   = -0.4f - energy * 0.8f;      // -0.4 → -1.2 dB
    float tLookahead = 2.0f  + arousal * 4.0f;     // 2 → 6 ms
    float tRelease   = 60.0f + arousal * 180.0f;   // 60 → 240 ms

    mCeiling   = 0.9f * mCeiling   + 0.1f * tCeiling;
    mLookahead = 0.9f * mLookahead + 0.1f * tLookahead;
    mRelease   = 0.9f * mRelease   + 0.1f * tRelease;
}
