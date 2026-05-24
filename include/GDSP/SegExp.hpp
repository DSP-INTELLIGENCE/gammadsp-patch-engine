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

class SegExp : public Generator {
public:
    SegExp(float lengthSeconds = 0.1f,
           float curvature = -3.f,
           float start = 0.f,
           float end   = 1.f)
    : 
    mSeg(lengthSeconds, curvature, start, end),
    mLength(lengthSeconds),
    mCurve(curvature),
    mStart(start),
    mEnd(end)  
    {
        
    }

    void set(float lengthSeconds, float curvature)
    {
        mLength = std::max(1e-6f, lengthSeconds);
        mCurve  = curvature;
        mSeg.set(mLength, mCurve);
    }

    void setLength(float seconds)
    {
        mLength = std::max(1e-6f, seconds);
        mSeg.length(mLength);
    }

    void setCurve(float curvature)
    {
        mCurve = curvature;
        mSeg.curve(mCurve);
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
        mSeg = end;
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

    float process()
    {
        return mSeg();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::SegExp<double, double> mSeg;

    float mLength;
    float mCurve;
    float mStart;
    float mEnd;
};



