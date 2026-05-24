#pragma once
#include <array>

template <class Sample, int MaxStates = 8, int MaxTransitions = 16>
class StateMachine {
public:
    using ConditionFn = bool (*)(Sample);

    struct Transition {
        int        from;
        int        to;
        ConditionFn condition;
    };

    StateMachine()
    {
        reset();
    }

    void reset()
    {
        mState = 0;
        mNumTransitions = 0;
    }

    int currentState() const { return mState; }

    void setInitialState(int s)
    {
        mState = s;
    }

    void addTransition(int from, int to, ConditionFn cond)
    {
        if (mNumTransitions < MaxTransitions)
            mTransitions[mNumTransitions++] = { from, to, cond };
    }

    // Call per control tick
    void process(Sample controlSignal)
    {
        for (int i = 0; i < mNumTransitions; ++i)
        {
            auto& t = mTransitions[i];
            if (t.from == mState && t.condition(controlSignal))
            {
                mState = t.to;
                break;
            }
        }
    }

private:
    int mState = 0;
    std::array<Transition, MaxTransitions> mTransitions;
    int mNumTransitions = 0;
};
