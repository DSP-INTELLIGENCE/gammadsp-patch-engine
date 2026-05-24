#pragma once
#include <algorithm>


class AdaptiveController {
public:
    AdaptiveController()
    {
        reset();
    }

    void reset()
    {
        mValue = (float)0;
    }

    // How fast the controller responds (0..1)
    void setResponsiveness(float r)
    {
        mResponse = std::clamp(r, (float)0.001, (float)1);
    }

    // Desired target coming from intelligence layer
    void setTarget(float t)
    {
        mTarget = t;
    }

    // Apply state weighting (for StateMachine integration)
    void setStateInfluence(float w)
    {
        mStateWeight = std::clamp(w, (float)0, (float)1);
    }

    // Per-sample control update
    float process()
    {
        // Weighted goal
        float goal = mTarget * mStateWeight;

        // Smooth adaptation
        mValue += mResponse * (goal - mValue);

        return mValue;
    }

    float value() const { return mValue; }

private:
    float mTarget      = (float)0;
    float mValue       = (float)0;
    float mResponse    = (float)0.05;
    float mStateWeight = (float)1;
};
