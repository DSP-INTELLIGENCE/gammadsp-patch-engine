#pragma once
#include "FormantMorphFilter.hpp"

class RadiationFilter {
public:
    RadiationFilter() = default;

    void reset() { mPrev = 0.0f; }

    // alpha ≈ 0.95 models lip radiation well
    void setCoefficient(float alpha)
    {
        mAlpha = alpha;
    }

    float process(float x)
    {
        float y = x - mAlpha * mPrev;
        mPrev = x;
        return y * mGain;
    }

private:
    float mPrev  = 0.0f;
    float mAlpha = 0.95f;
    float mGain  = 0.5f;
};

class VocalTract {
public:
    float process()
    {
        float src = generateSource();
        float tract = formant.process(src);
        float mouth = radiation.process(tract);
        return mouth;
    }

private:
    FormantMorphFilter formant;
    RadiationFilter radiation;
};
