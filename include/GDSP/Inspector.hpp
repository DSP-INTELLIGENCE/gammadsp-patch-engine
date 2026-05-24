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


class Inspector: public Function {
public:
    
    Inspector()
    : mInspector()
    {
    }

    float process(float input) override
    {
        return (float)mInspector(input);
    }

    void run(const float* input, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            process(input[i]);
    }

    void reset()
    {
        mInspector.reset();
    }

    void setDCThreshold(float v)
    {
        mInspector.DCThresh(v);
    }

    void setName(const char* name)
    {
        mInspector.name(name);
    }

    unsigned status() const
    {
        return mInspector.status();
    }

    bool hasError() const
    {
        return status() != 0;
    }

    float peak() const
    {
        return mInspector.peak();
    }

    float dc() const
    {
        return mInspector.DC();
    }

    float maxDC() const
    {
        return mInspector.maxDC();
    }

    const char* name() const
    {
        return mInspector.name();
    }

    void Print(bool showName) const
    {
        mInspector.print(showName);
    }

private:
    gam::Inspector mInspector;
};
