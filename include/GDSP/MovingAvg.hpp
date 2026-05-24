#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <atomic>
#include <algorithm>

#include "Engine.hpp"
#include "Parameters.hpp"

class MovingAvg : public Function {
public:
    MovingAvg(unsigned size = 64)
    : mFilter(size)
    {
        
    }

    void resize(unsigned size)
    {
        mFilter.resize(size);
    }

    float process(float input)
    {
        return mFilter(input);
    }

    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }

private:
    gam::MovingAvg<double> mFilter;
};

