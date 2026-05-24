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

class Seg : public Generator {
public:
    Seg(float lengthSeconds = 0.1f,
        float start = 0.f,
        float end   = 1.f,
        float phase = 0.f)
    : mSeg(lengthSeconds, start, end, phase),
    mLength(lengthSeconds),
    mStart(start),
    mEnd(end)  
    {
        
    }

    void setLength(float seconds)
    {
        mLength = std::max(1e-6f, seconds);
        mSeg.length(mLength);
    }

    void setFreq(float hz)
    {
        mSeg.freq(std::max(0.f, hz));
    }

    void setPhase(float phase)
    {
        mSeg.phase(std::clamp(phase, 0.f, 0.999999f));
    }

    void setLevels(float start, float end)
    {
        mStart = start;
        mEnd   = end;
        mSeg.levels(mStart, mEnd);
    }

    void setEnd(float end)
    {
        mEnd = end;
        mSeg = end;  // Gamma: smooth transition from current value
    }

    void reset()
    {
        mSeg.reset();
    }

    bool done() const
    {
        return mSeg.done();
    }

    float value() const
    {
        return mSeg.value();
    }

    float process() override
    {
        return mSeg();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::Seg<double> mSeg;

    float mLength;
    float mStart;
    float mEnd;
};

