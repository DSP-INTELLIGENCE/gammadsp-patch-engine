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

class SineDs : public Generator {
public:
    
    SineDs(unsigned count = 1)
    : mBank(count)
    {
        
    }

    void resize(unsigned count)
    {
        mBank.resize(count);
    }

    void set(unsigned index,
                    float freq,
                    float amp,
                    float decay60,
                    float phase=0.0f)
    {
        if(index < mBank.size())
            mBank.set(index, freq, amp, decay60, phase);
    }

    float process() override
    {
        return mBank();
    }

    float last(unsigned index) const
    {
        if(index < mBank.size())
            return mBank.last(index);
        return 0.0;
    }

    unsigned size() const
    {
        return mBank.size();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }


private:
    gam::SineDs<double> mBank;
};



