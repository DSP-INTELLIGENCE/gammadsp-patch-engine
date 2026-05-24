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

class Curve : public Generator {
public:
    Curve(float lengthSeconds = 0.1f,
          float curvature = -4.0f,
          float start = 0.0f,
          float end   = 1.0f)
    : mLength(lengthSeconds),
    mCurvature(curvature),
    mStart(start),
    mEnd(end)
    {    
        mCurve.set(mLength * gam::sampleRate(), mCurvature, mStart, mEnd);
    }

    void set(float lengthSeconds, float curvature, float start, float end)
    {
        mLength    = std::max(0.f, lengthSeconds);
        mCurvature = curvature;
        mStart     = start;
        mEnd       = end;

        mCurve.set(mLength * gam::sampleRate(), mCurvature, mStart, mEnd);
    }

    void setLength(float lengthSeconds)
    {
        mLength = std::max(0.f, lengthSeconds);
        mCurve.set(mLength * gam::sampleRate(), mCurvature, mStart, mEnd);
    }

    void setCurvature(float curvature)
    {
        mCurvature = curvature;
        mCurve.set(mLength * gam::sampleRate(), mCurvature, mStart, mEnd);
    }

    void setLevels(float start, float end)
    {
        mStart = start;
        mEnd   = end;
        mCurve.levels(mStart, mEnd);
    }

    void reset()
    {
        mCurve.reset();
    }

    bool done() const
    {
        return mCurve.done();
    }

    float value() const
    {
        return mCurve.value();
    }

    float process() override
    {
        return mCurve();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

    ~Curve() {}
    
    float mLength;
    float mCurvature;
    float mStart;
    float mEnd;

private:
    gam::Curve<double, double> mCurve;

    
};



