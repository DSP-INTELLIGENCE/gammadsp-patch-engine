#pragma once
#include <array>
#include <algorithm>

template <class Sample, int MaxBehaviors = 8>
class BehaviorModel {
public:
    struct Behavior {
        Sample energyBias   = (Sample)0;   // -1..1
        Sample brightnessBias = (Sample)0; // -1..1
        Sample tempoBias    = (Sample)0;   // -1..1
        Sample motionBias   = (Sample)0;   // -1..1
        Sample mood         = (Sample)0;   // -1..1
    };

    BehaviorModel()
    {
        reset();
    }

    void reset()
    {
        mActive = 0;
        for (auto& b : mBehaviors) b = {};
    }

    void defineBehavior(int index, const Behavior& b)
    {
        if (index >= 0 && index < MaxBehaviors)
            mBehaviors[index] = b;
    }

    void setActive(int index)
    {
        if (index >= 0 && index < MaxBehaviors)
            mActive = index;
    }

    const Behavior& active() const
    {
        return mBehaviors[mActive];
    }

    // Blend toward active behavior over time
    Behavior process()
    {
        const Behavior& target = mBehaviors[mActive];

        // Smooth transitions
        mCurrent.energyBias    += (Sample)0.02 * (target.energyBias    - mCurrent.energyBias);
        mCurrent.brightnessBias+= (Sample)0.02 * (target.brightnessBias- mCurrent.brightnessBias);
        mCurrent.tempoBias     += (Sample)0.02 * (target.tempoBias     - mCurrent.tempoBias);
        mCurrent.motionBias    += (Sample)0.02 * (target.motionBias    - mCurrent.motionBias);
        mCurrent.mood          += (Sample)0.02 * (target.mood          - mCurrent.mood);

        return mCurrent;
    }

private:
    std::array<Behavior, MaxBehaviors> mBehaviors;
    int mActive = 0;

    Behavior mCurrent;
};
