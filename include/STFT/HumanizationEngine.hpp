#pragma once
#include <cmath>

class HumanizationEngine {
public:
    HumanizationEngine(float sampleRate);

    void reset();

    // called once per musical event (note, hit, phrase)
    void process(float expression,
                 float grooveTightness,
                 float grooveSwing,
                 float arousal,
                 float movement);

    float timingOffset()  const { return mTimingOffset; }   // seconds
    float velocityScale() const { return mVelocityScale; }  // 0..1+
    float articulation()  const { return mArticulation; }   // 0..1

private:
    float mSampleRate;

    float mTimingOffset;
    float mVelocityScale;
    float mArticulation;
};
