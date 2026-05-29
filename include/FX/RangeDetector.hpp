#pragma once
#include <algorithm>

template <class Sample>
class RangeDetector {
public:
    RangeDetector(Sample low = (Sample)0, Sample high = (Sample)1)
    : mLow(low), mHigh(high)
    {
        normalize();
    }

    void setRange(Sample low, Sample high)
    {
        mLow = low;
        mHigh = high;
        normalize();
    }

    void setLow(Sample low)
    {
        mLow = low;
        normalize();
    }

    void setHigh(Sample high)
    {
        mHigh = high;
        normalize();
    }

    void reset()
    {
        mValue = (Sample)0;
        mInside = false;
    }

    // Per-sample: returns normalized position inside range (0..1), 0 if outside
    Sample process(Sample x)
    {
        if (x >= mLow && x <= mHigh)
        {
            mInside = true;
            mValue = (x - mLow) / (mHigh - mLow);
        }
        else
        {
            mInside = false;
            mValue = (Sample)0;
        }

        return mValue;
    }

    bool inside() const { return mInside; }
    Sample value() const { return mValue; }

private:
    void normalize()
    {
        if (mLow > mHigh)
            std::swap(mLow, mHigh);

        if (mHigh - mLow < (Sample)1e-9)
            mHigh = mLow + (Sample)1e-9;
    }

private:
    Sample mLow;
    Sample mHigh;

    Sample mValue = (Sample)0;
    bool   mInside = false;
};
