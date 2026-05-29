#pragma once
#include "ExpressionAnalyzer.hpp"

class EmotionModel {
public:
    
    EmotionModel()
    {
        reset();
    }

    void reset()
    {
        mTension = 0.0f;
        mCalm = 1.0f;
        mAggression = 0.0f;
        mWarmth = 0.5f;
        mMotion = 0.0f;
        mFocus = 0.5f;
    }

    void process(const ExpressionAnalyzer& e)
    {
        float I = e.intensity();
        float A = e.articulation();
        float S = e.stability();
        float B = e.brightness();
        float X = e.expressiveness();

        // Emotional mappings
        float tension    = 0.5f * I + 0.3f * A + 0.2f * B;
        float calm       = (1.0f - I) * 0.6f + S * 0.4f;
        float aggression = 0.6f * I + 0.4f * A;
        float warmth     = (1.0f - B) * 0.7f + S * 0.3f;
        float motion     = 0.5f * A + 0.3f * I + 0.2f * (1.0f - S);
        float focus      = 0.6f * S + 0.4f * (1.0f - A);

        // Smooth emotional state
        mTension    = 0.97f * mTension    + 0.03f * tension;
        mCalm       = 0.97f * mCalm       + 0.03f * calm;
        mAggression = 0.97f * mAggression + 0.03f * aggression;
        mWarmth     = 0.97f * mWarmth     + 0.03f * warmth;
        mMotion     = 0.97f * mMotion     + 0.03f * motion;
        mFocus      = 0.97f * mFocus      + 0.03f * focus;

        // Clamp for safety
        mTension    = std::clamp(mTension,    0.0f, 1.0f);
        mCalm       = std::clamp(mCalm,       0.0f, 1.0f);
        mAggression = std::clamp(mAggression, 0.0f, 1.0f);
        mWarmth     = std::clamp(mWarmth,     0.0f, 1.0f);
        mMotion     = std::clamp(mMotion,     0.0f, 1.0f);
        mFocus      = std::clamp(mFocus,      0.0f, 1.0f);
    }

    float tension()    const { return mTension; }
    float calm()       const { return mCalm; }
    float aggression() const { return mAggression; }
    float warmth()     const { return mWarmth; }
    float motion()     const { return mMotion; }
    float focus()      const { return mFocus; }

private:
    float mTension;
    float mCalm;
    float mAggression;
    float mWarmth;
    float mMotion;
    float mFocus;
};
