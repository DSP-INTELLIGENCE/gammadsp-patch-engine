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


class ZeroCrossRate: public Function {
public:
    ZeroCrossRate(unsigned windowSize = 256)
    : mZCR(windowSize)
    {
    }

    void reset()
    {
        mZCR.reset();
    }

    void setWindow(unsigned windowSize)
    {
        mZCR.period(windowSize);
    }

    float process(float input) override
    {
        return mZCR(input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    float value() const
    {
        return mZCR.value();
    }


private:
    gam::ZeroCrossRate<double> mZCR;
};




