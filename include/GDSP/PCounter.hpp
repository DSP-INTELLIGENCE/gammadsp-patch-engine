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


class PCounter: public Generator {
public:
    PCounter(unsigned period = 256)
    : mCounter(period)
    {
    }

    void setPeriod(unsigned period)
    {
        mCounter.period(period);
    }

    unsigned period() const
    {
        return mCounter.period();
    }

    unsigned count() const
    {
        return mCounter.count();
    }

    bool cycled() const
    {
        return mCounter.cycled();
    }

    void reset()
    {
        mCounter.reset();
    }

    float process() override
    {
        return (float)mCounter();
    }

    void run(bool* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }


private:
    gam::PCounter mCounter;
};


