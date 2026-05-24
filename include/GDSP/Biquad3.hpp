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

class Biquad3 : public Function {
public:
    using FilterType = gam::FilterType;

    /// Filter types (mirrors gam::FilterType)    
    Biquad3(float f0, float f1, float f2,
            float res = 8.0f,            
            float level = 1.0f,
            FilterType type = FilterType::BAND_PASS)
    : 
    mFilter(f0, f1, f2, res, type)
    {    
    
    }


    void setFreqs(float f0, float f1, float f2)
    {
        float mF0 = std::max(5.f, f0);
        float mF1 = std::max(5.f, f1);
        float mF2 = std::max(5.f, f2);

        mFilter.freq(mF0, mF1, mF2);
    }

    void setRes(float res)
    {
        float mRes = std::max(0.001f, res);
        mFilter.bq0.res(mRes);
        mFilter.bq1.res(mRes);
        mFilter.bq2.res(mRes);
    }

    void setLevel(float level)
    {
        float mLevel = std::max(0.f, level);
        mFilter.bq0.level(mLevel);
        mFilter.bq1.level(mLevel);
        mFilter.bq2.level(mLevel);
    }

    void setType(FilterType type, int filter)
    {    
        switch(filter)
        {
            case 0: mFilter.bq0.type(type); break;
            case 1: mFilter.bq1.type(type); break;
            case 2: mFilter.bq2.type(type); break;
            default:
                mFilter.bq0.type(type); break;
                mFilter.bq1.type(type); break;
                mFilter.bq2.type(type); break;        
        }
    }

    void reset()
    {
        mFilter.bq0.reset();
        mFilter.bq1.reset();
        mFilter.bq2.reset();
    }

    float process(float input) override
    {
        return mFilter(input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }


    void setType0(FilterType type) { mFilter.bq0.type(type); }
    void setType1(FilterType type) { mFilter.bq1.type(type); }
    void setType2(FilterType type) { mFilter.bq2.type(type); }

private:
    gam::Biquad3 mFilter;  
};


