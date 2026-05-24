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


class ZeroCross: public Function {
public:
    ZeroCross(float initialValue = 0.0f)
    : mDetector(initialValue)
    {
    }

    // Process one sample
    // Returns:  1  → rising zero-cross
    //           0  → no crossing
    //          -1  → falling zero-cross
    
    float process(float input) override
    {
        return (float)mDetector(input);
    }

    void run(const float* input, int* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    
private:
    gam::ZeroCross<double> mDetector;
};


