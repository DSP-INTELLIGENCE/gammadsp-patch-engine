#include "MoodState.hpp"
#include <algorithm>

MoodState::MoodState()
{
    reset();
}

void MoodState::reset()
{
    mSuspense = 0.0f;
    mRelaxation = 1.0f;
    mDanger = 0.0f;
    mHeroism = 0.5f;
    mSadness = 0.0f;
    mHope = 0.5f;
}

void MoodState::process(const EmotionModel& e)
{
    float tension    = e.tension();
    float calm       = e.calm();
    float aggression = e.aggression();
    float warmth     = e.warmth();
    float motion     = e.motion();
    float focus      = e.focus();

    float suspense   = 0.6f * tension + 0.4f * (1.0f - focus);
    float danger     = 0.7f * aggression + 0.3f * tension;
    float relaxation = 0.7f * calm + 0.3f * warmth;
    float heroism    = 0.5f * motion + 0.3f * focus + 0.2f * (1.0f - tension);
    float sadness    = (1.0f - motion) * 0.5f + (1.0f - warmth) * 0.5f;
    float hope       = 0.6f * warmth + 0.4f * (1.0f - danger);

    // very slow smoothing: mood has memory
    mSuspense   = 0.995f * mSuspense   + 0.005f * suspense;
    mDanger     = 0.995f * mDanger     + 0.005f * danger;
    mRelaxation = 0.995f * mRelaxation + 0.005f * relaxation;
    mHeroism    = 0.995f * mHeroism    + 0.005f * heroism;
    mSadness    = 0.995f * mSadness    + 0.005f * sadness;
    mHope       = 0.995f * mHope       + 0.005f * hope;

    // safety clamp
    mSuspense   = std::clamp(mSuspense,   0.0f, 1.0f);
    mDanger     = std::clamp(mDanger,     0.0f, 1.0f);
    mRelaxation = std::clamp(mRelaxation, 0.0f, 1.0f);
    mHeroism    = std::clamp(mHeroism,    0.0f, 1.0f);
    mSadness    = std::clamp(mSadness,    0.0f, 1.0f);
    mHope       = std::clamp(mHope,       0.0f, 1.0f);
}
