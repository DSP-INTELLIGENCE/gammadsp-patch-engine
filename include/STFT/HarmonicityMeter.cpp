#include "HarmonicityMeter.hpp"
#include <algorithm>

HarmonicityMeter::HarmonicityMeter(float sr)
: mSampleRate(sr),
  mHarmonicity(0.0f)
{}

void HarmonicityMeter::reset()
{
    mHarmonicity = 0.0f;
}

float HarmonicityMeter::process(float pitch, float stability)
{
    float target = 0.0f;

    // Tonal energy exists only when pitch is present & stable
    if(pitch > 0.0f && stability > 0.6f)
    {
        // Stronger when pitch is very stable
        target = std::min((stability - 0.6f) * 2.5f, 1.0f);
    }

    // Smooth like a musical meter
    mHarmonicity = 0.92f * mHarmonicity + 0.08f * target;

    return mHarmonicity;
}
