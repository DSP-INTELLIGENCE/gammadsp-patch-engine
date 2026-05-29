#pragma once
#include "EmotionModel.hpp"

class MoodState {
public:
    MoodState();

    void reset();

    // call every control frame (~20–60 Hz)
    void process(const EmotionModel& e);

    float suspense() const { return mSuspense; }
    float relaxation() const { return mRelaxation; }
    float danger() const { return mDanger; }
    float heroism() const { return mHeroism; }
    float sadness() const { return mSadness; }
    float hope() const { return mHope; }

private:
    float mSuspense;
    float mRelaxation;
    float mDanger;
    float mHeroism;
    float mSadness;
    float mHope;
};


