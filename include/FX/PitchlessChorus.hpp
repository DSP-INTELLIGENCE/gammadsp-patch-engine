#pragma once
#include "FrequencyShifer.hpp"

class PitchlessChorus : public Function {
public:
    PitchlessChorus()
    : s1( 6.f), s2(-9.f), s3( 12.f)
    {
        setMix(0.5f);
    }

    void setMix(float m) { mix = std::clamp(m, 0.f, 1.f); }

    void reset()
    {
        s1.reset(); s2.reset(); s3.reset();
    }

    float process(float x) override
    {
        float a = s1.process(x);
        float b = s2.process(x);
        float c = s3.process(x);

        float y = (a + b + c) * 0.3333f;

        return x * (1.f - mix) + y * mix;
    }

private:
    FrequencyShifter s1, s2, s3;
    float mix = 0.5f;
};
