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


class Threshold: public Function {
public:
    Threshold(float threshold = 0.5f, float smoothFreq = 10.0f)
    : 
    mValue(0.0f),
    mThresh(threshold, smoothFreq)
    {
    }

    void setThreshold(float v)
    {
        mThresh.thresh = v;
    }

    void setSmoothFreq(float freqHz)
    {
        mThresh.lpf.freq(std::max(0.01f, freqHz));
    }

    void reset(float value=0.0f)
    {
        mValue = value;
        mThresh.lpf.reset(value);
    }

    float process(float input) override
    {
        mValue = mThresh(input);
        return mValue;
    }

    float process(float input, float highValue, float lowValue)
    {
        mValue = mThresh(input, highValue, lowValue);
        return mValue;
    }

    float processInv(float input)
    {
        mValue = mThresh.inv(input);
        return mValue;
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    
    
    float mThreshold = 0.5f;
    float mSmooth    = 0.5f;    
    float mValue     = 0.0f;   // cached last output

private:
    gam::Threshold<double> mThresh;
    
};

