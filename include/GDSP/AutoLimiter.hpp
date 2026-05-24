#pragma once

class AutoLimiter {
public:
    AutoLimiter();

    void reset();

    void process(float energy, float arousal);

    float ceiling()   const { return mCeiling; }   // dB
    float lookahead() const { return mLookahead; } // ms
    float release()   const { return mRelease; }   // ms

private:
    float mCeiling;
    float mLookahead;
    float mRelease;
};
