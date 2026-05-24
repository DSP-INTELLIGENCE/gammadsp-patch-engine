#pragma once
#include <cmath>

class HarmonicityMeter {
public:
    HarmonicityMeter(float sampleRate);

    void reset();

    // feed current pitch & stability
    float process(float pitch, float stability);

    // 0..1 — how harmonic / tonal the signal is
    float harmonicity() const { return mHarmonicity; }

private:
    float mSampleRate;

    float mHarmonicity;
};
