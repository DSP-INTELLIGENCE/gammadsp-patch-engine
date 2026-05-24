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

class BlockDC : public Function {
public:
    BlockDC(float width = 35.0f)
    : mFilter(width)
    {
        
    }

    void setWidth(float width)
    {
        mFilter.width(width);
    }

    void reset()
    {
        mFilter.reset();
    }

    float process(float input) override
    {
        return mFilter(input);
    }

    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }
        
private:
    gam::BlockDC<double> mFilter;
};

