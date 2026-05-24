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
#include <algorithm>

#include "Engine.hpp"
#include "Parameters.hpp"

class Integrator : public Function {
public:
    Integrator(float leak = 1.0f,
               float init = 0.0f)
    : mFilter(leak, init)
    {
        
    }

    void setLeak(float leak)
    {
        mFilter.leak(leak);
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
    gam::Integrator<double> mFilter;
    
};

