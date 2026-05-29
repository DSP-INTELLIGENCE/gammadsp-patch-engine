#pragma once
#include <algorithm>

enum NarrativePhase {
    NARRATIVE_INTRO,
    NARRATIVE_BUILD,
    NARRATIVE_CLIMAX,
    NARRATIVE_RELEASE,
    NARRATIVE_OUTRO
};

class NarrativeState {
public:
    NarrativeState();

    void reset();

    // Call at control rate (e.g. 20–60 Hz)
    void process(float tension, float energy, float motion, float moodDanger, float moodHope);

    NarrativePhase phase() const { return mPhase; }
    float arcPosition() const { return mArc; }       // 0..1
    float dramaticWeight() const { return mDrama; }  // 0..1

private:
    NarrativePhase mPhase;
    float mArc;
    float mDrama;
};
