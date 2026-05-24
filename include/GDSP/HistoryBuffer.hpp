#pragma once
#include <array>
#include <algorithm>

template <class Sample, int Length = 1024>
class HistoryBuffer {
public:
    HistoryBuffer()
    {
        reset();
    }

    void reset()
    {
        mIndex = 0;
        mFilled = false;
        mSum = (Sample)0;
        for (auto& v : mData) v = (Sample)0;
    }

    // Push new value (per control tick or per sample)
    void push(Sample x)
    {
        if (mFilled)
            mSum -= mData[mIndex];

        mData[mIndex] = x;
        mSum += x;

        mIndex = (mIndex + 1) % Length;
        if (mIndex == 0) mFilled = true;
    }

    // Smoothed average over history
    Sample average() const
    {
        int n = mFilled ? Length : mIndex;
        if (n == 0) return (Sample)0;
        return mSum / (Sample)n;
    }

    // Access delayed value (0 = most recent)
    Sample get(int delay) const
    {
        if (delay < 0) delay = 0;
        if (delay >= Length) delay = Length - 1;

        int idx = mIndex - 1 - delay;
        if (idx < 0) idx += Length;
        return mData[idx];
    }

private:
    std::array<Sample, Length> mData;

    int   mIndex = 0;
    bool  mFilled = false;
    Sample mSum = (Sample)0;
};
