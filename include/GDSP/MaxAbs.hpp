#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif
#include <cassert>
#include <vector>
#include <memory>
#include <atomic>
#include "Engine.hpp"
#include "Parameters.hpp"


class MaxAbs: public Function {
public:
    MaxAbs(unsigned windowSize = 256)
    : mMax(windowSize)
    {
    }

    void reset()
    {
        mMax.reset();
    }

    void setWindow(unsigned windowSize)
    {
        mMax.period(windowSize);
    }

    float process(float input) override
    {
        return mMax(input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    float value() const
    {
        return mMax.value();
    }

    float valueN() const
    {
        return mMax.valueN();
    }

private:
    gam::MaxAbs<double> mMax;
};



