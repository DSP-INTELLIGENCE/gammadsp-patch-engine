#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class MotionCurveGenerator {
public:
    enum Curve {
        Linear,
        EaseIn,
        EaseOut,
        EaseInOut,
        Exponential,
        Sinusoidal
    };

    MotionCurveGenerator()
    {
        reset();
    }

    void reset()
    {
        mPhase = (Sample)0;
        mActive = false;
    }

    void start(Sample durationSeconds, Curve c)
    {
        mDuration = std::max(durationSeconds, (Sample)1e-6);
        mCurve = c;
        mPhase = (Sample)0;
        mActive = true;
    }

    bool active() const { return mActive; }

    // Per-sample
    Sample process()
    {
        if (!mActive) return mValue;

        Sample step = (Sample)1 / (mDuration * (Sample)gam::sampleRate());
        mPhase += step;

        if (mPhase >= (Sample)1)
        {
            mPhase = (Sample)1;
            mActive = false;
        }

        mValue = shape(mPhase);
        return mValue;
    }

    Sample value() const { return mValue; }

private:
    Sample shape(Sample x) const
    {
        switch (mCurve)
        {
        default:
        case Linear:      return x;
        case EaseIn:      return x * x;
        case EaseOut:     return (Sample)1 - (Sample)std::pow((Sample)1 - x, (Sample)2);
        case EaseInOut:   return x * x * ((Sample)3 - (Sample)2 * x);
        case Exponential: return std::pow(x, (Sample)3);
        case Sinusoidal:  return (Sample)0.5 - (Sample)0.5 * std::cos(x * (Sample)M_PI);
        }
    }

private:
    Sample mDuration = (Sample)1;
    Sample mPhase    = (Sample)0;
    Sample mValue    = (Sample)0;
    Curve  mCurve    = Linear;
    bool   mActive   = false;
};
