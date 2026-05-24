#pragma once
#include <cmath>

class ScaleKeyTracker {
public:
    ScaleKeyTracker(float sampleRate);

    void reset();

    // feed current pitch & harmonicity
    void process(float pitch, float harmonicity);

    int  key()  const { return mKey; }   // 0..11 (C..B)
    bool minor() const { return mMinor; }
    float confidence() const { return mConfidence; }

private:
    float mSampleRate;

    float mHistogram[12];

    int   mKey;
    bool  mMinor;
    float mConfidence;

    void evaluate();
};
