#include "NarrativeState.hpp"

NarrativeState::NarrativeState()
{
    reset();
}

void NarrativeState::reset()
{
    mPhase = NARRATIVE_INTRO;
    mArc = 0.0f;
    mDrama = 0.0f;
}

void NarrativeState::process(float tension, float energy, float motion, float danger, float hope)
{
    // Progress arc slowly
    mArc += 0.0003f;
    mArc = std::min(mArc, 1.0f);

    // Phase transitions
    if(mArc < 0.2f) mPhase = NARRATIVE_INTRO;
    else if(mArc < 0.5f) mPhase = NARRATIVE_BUILD;
    else if(mArc < 0.7f) mPhase = NARRATIVE_CLIMAX;
    else if(mArc < 0.9f) mPhase = NARRATIVE_RELEASE;
    else mPhase = NARRATIVE_OUTRO;

    // Dramatic weight computation
    float targetDrama =
        0.4f * tension +
        0.3f * energy +
        0.2f * danger +
        0.1f * motion -
        0.3f * hope;

    mDrama = 0.98f * mDrama + 0.02f * std::clamp(targetDrama, 0.0f, 1.0f);
}
