#pragma once
#include "TPT1Pole.hpp"
#include "Biquad.hpp"

struct ToneStack
{
    virtual ~ToneStack() = default;

    virtual void prepare(float sr) = 0;
    virtual void reset() = 0;

    // tone01 = 0..1
    virtual float process(float x, float tone01) = 0;
};
