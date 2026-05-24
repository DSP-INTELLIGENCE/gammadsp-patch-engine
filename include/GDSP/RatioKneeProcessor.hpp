#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class RatioKneeProcessor {
public:
    RatioKneeProcessor()
    {
        reset();
    }

    void reset()
    {
        mLastGain = (Sample)1;
    }

    // Parameters
    void setThreshold(Sample t) { mThreshold = t; }
    void setRatio(Sample r)     { mRatio = std::max((Sample)1, r); }
    void setKnee(Sample k)      { mKnee = std::max((Sample)0, k); }
    void setMaxBoost(Sample g)  { mMaxBoost = std::max((Sample)1, g); }
    void setMaxCut(Sample g)    { mMaxCut = std::clamp(g, (Sample)1e-6, (Sample)1); }

    // Core transfer function
    Sample process(Sample x)
    {
        Sample level = std::max(std::abs(x), (Sample)1e-9);
        Sample g = computeGain(level);
        mLastGain = g;
        return x * g;
    }

    Sample gain() const { return mLastGain; }

private:
    Sample computeGain(Sample L)
    {
        Sample t = mThreshold;
        Sample r = mRatio;
        Sample k = mKnee;

        auto above = [&](Sample x){
            Sample y = t + (x - t) / r;
            return y / x;
        };

        auto below = [&](Sample x){
            Sample y = t * std::pow(x / t, (Sample)1 / r);
            return y / x;
        };

        Sample g;

        if (k <= (Sample)0)
        {
            g = (L >= t) ? above(L) : below(L);
        }
        else
        {
            Sample half = (Sample)0.5 * k;
            Sample lo = t - half;
            Sample hi = t + half;

            if      (L <= lo) g = below(L);
            else if (L >= hi) g = above(L);
            else
            {
                Sample u = (L - lo) / (hi - lo);
                Sample s = u * u * ((Sample)3 - (Sample)2 * u);
                g = ((Sample)1 - s) * below(L) + s * above(L);
            }
        }

        g = std::min(g, mMaxBoost);
        g = std::max(g, mMaxCut);
        return g;
    }

private:
    Sample mThreshold = (Sample)0.1;
    Sample mRatio     = (Sample)2;
    Sample mKnee      = (Sample)0;
    Sample mMaxBoost  = (Sample)4;
    Sample mMaxCut    = (Sample)0.05;

    Sample mLastGain  = (Sample)1;
};
