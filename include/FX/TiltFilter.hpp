#pragma once
#include "RBJFilter.hpp"

template<class Sample>
class TiltFilter {
public:
    TiltFilter(Sample sampleRate, Sample pivot = Sample(1000))
    : mLow(sampleRate, RBJFilter<Sample>::LOW_SHELF),
      mHigh(sampleRate, RBJFilter<Sample>::HIGH_SHELF)
    {
        setPivot(pivot);
        setSlope(Sample(1));
        setTiltDb(Sample(0));
    }

    void setPivot(Sample f) {
        mLow.setFreq(f);
        mHigh.setFreq(f);
    }

    void setSlope(Sample s) {
        mLow.setShelfSlope(s);
        mHigh.setShelfSlope(s);
    }

    void setTiltDb(Sample db) {
        mLow.setGainDb(-db);
        mHigh.setGainDb(+db);
    }

    Sample process(Sample x) {
        x = mLow.process(x);
        x = mHigh.process(x);
        return x;
    }

    void reset() { mLow.reset(); mHigh.reset(); }

private:
    RBJFilter<Sample> mLow, mHigh;
};
