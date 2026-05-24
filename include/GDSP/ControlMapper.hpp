#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class ControlMapper {
public:
    enum Mapping {
        Linear,
        Exponential,
        Logarithmic,
        Decibel,
        Semitone
    };

    ControlMapper() = default;

    void setRange(Sample inMin, Sample inMax, Sample outMin, Sample outMax)
    {
        mInMin  = inMin;
        mInMax  = inMax;
        mOutMin = outMin;
        mOutMax = outMax;
    }

    void setMapping(Mapping m)
    {
        mMapping = m;
    }

    Sample process(Sample x) const
    {
        // Normalize input
        Sample u = (x - mInMin) / (mInMax - mInMin);
        u = std::clamp(u, (Sample)0, (Sample)1);

        // Apply mapping curve
        switch (mMapping)
        {
        case Linear:      break;

        case Exponential: u = u * u; break;

        case Logarithmic: u = std::log10((Sample)1 + (Sample)9 * u); break;

        case Decibel:     u = std::pow((Sample)10, (u - (Sample)1) * (Sample)2); break;

        case Semitone:    u = std::pow((Sample)2, (u * (Sample)12 - (Sample)6) / (Sample)12); break;
        }

        // Scale to output
        return mOutMin + u * (mOutMax - mOutMin);
    }

private:
    Sample  mInMin  = (Sample)0;
    Sample  mInMax  = (Sample)1;
    Sample  mOutMin = (Sample)0;
    Sample  mOutMax = (Sample)1;
    Mapping mMapping = Linear;
};
