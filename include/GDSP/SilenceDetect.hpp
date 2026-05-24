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


class SilenceDetect : public Function {
public:

    SilenceDetect(unsigned count = 1000)    
    : mDetect(count)
    {
    }

    void setCount(unsigned count)
    {
        mDetect.count(count);
    }

    void reset()
    {
        mDetect.reset();
    }

    float process(float input) override
    {
        return (float)mDetect(input, threshold);
    }

    bool done() const
    {
        return mDetect.done();
    }

    void run(const float* input, bool* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    void setThreshold(float v) { threshold = v; }


    float threshold = 0.001f;
    uint32_t count  = 1000;
    
private:
    gam::SilenceDetect mDetect;
    
};

