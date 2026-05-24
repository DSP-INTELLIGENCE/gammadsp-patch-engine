#pragma once
#include <algorithm>

class ChordState {
public:
    ChordState();
    {
        reset();
    }

    void reset()
    {
        mKeyCenter = -1;
        mFunction = 0;
        mTension = 0.0f;
        mConfidence = 0.0f;
        mPrevPitchClass = -1;
    }

    void process(int pc, float harmonicity, float consonance)
    {
        if(pc < 0 || harmonicity < 0.2f)
        {
            mConfidence *= 0.98f;
            return;
        }

        // Key center stabilization
        if(mKeyCenter < 0)
            mKeyCenter = pc;

        int rel = (pc - mKeyCenter + 12) % 12;

        // Rough harmonic function mapping
        int function;
        if(rel == 0 || rel == 7)          function = 0; // Tonic (I, V)
        else if(rel == 5 || rel == 2)     function = 1; // Subdominant (IV, ii)
        else                              function = 2; // Dominant / tension

        // Update confidence
        if(pc == mPrevPitchClass)
            mConfidence += 0.02f;
        else
            mConfidence -= 0.01f;

        mConfidence = std::clamp(mConfidence, 0.0f, 1.0f);

        // Tension estimate
        float baseTension = 1.0f - consonance;
        if(function == 2) baseTension += 0.2f;
        if(function == 0) baseTension -= 0.2f;

        mTension = 0.95f * mTension + 0.05f * std::clamp(baseTension, 0.0f, 1.0f);

        mFunction = function;
        mPrevPitchClass = pc;
    }

    
    int   keyCenter() const { return mKeyCenter; }      // 0..11
    int   function() const { return mFunction; }       // 0=Tonic,1=Subdominant,2=Dominant
    float tension() const { return mTension; }         // 0..1
    float confidence() const { return mConfidence; }

private:
    int   mKeyCenter;
    int   mFunction;

    float mTension;
    float mConfidence;

    int   mPrevPitchClass;
};
