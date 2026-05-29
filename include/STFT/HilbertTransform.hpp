#pragma once
#include <cmath>
#include <algorithm>


struct HilbertIQ {
    float i;   // in-phase
    float q;   // quadrature (~ +90°)
};

template <class float>
class HilbertTransform {
public:
    HilbertTransform()
    {
        // Olli Niemitalo 4-section allpass pair
        const float a1[4] = {
            (float)0.6923878,
            (float)0.9360654322959,
            (float)0.9882295226860,
            (float)0.9987488452737
        };

        const float a2[4] = {
            (float)0.4021921162426,
            (float)0.8561710882420,
            (float)0.9722909545651,
            (float)0.9952884791278
        };

        for (int i = 0; i < 4; ++i) {
            mAP1[i].setA(a1[i]);
            mAP2[i].setA(a2[i]);
        }

        reset();
    }

    void reset()
    {
        for (int i = 0; i < 4; ++i) {
            mAP1[i].reset();
            mAP2[i].reset();
        }
        mDelay1.reset();
        mLastI = (float)0;
        mLastQ = (float)0;
    }

    // Per-sample: returns analytic I/Q pair
    HilbertIQ<float> process(float x)
    {
        // Quadrature branch
        float y1 = x;
        for (int i = 0; i < 4; ++i)
            y1 = mAP1[i].process(y1);
        y1 = mDelay1.process(y1);

        // In-phase branch
        float y2 = x;
        for (int i = 0; i < 4; ++i)
            y2 = mAP2[i].process(y2);

        mLastI = y2 * mScale;
        mLastQ = y1 * mScale;

        return { mLastI, mLastQ };
    }

    float i() const { return mLastI; }
    float q() const { return mLastQ; }

    float amplitude() const
    {
        return std::sqrt(mLastI * mLastI + mLastQ * mLastQ);
    }

    float phase() const
    {
        return std::atan2(mLastQ, mLastI);
    }

    void setScale(float s) { mScale = s; }

private:
    struct AllpassZ2 {
        float a2 = 0;
        float x1 = 0, x2 = 0;
        float y1 = 0, y2 = 0;

        void setA(float a) { a2 = a * a; }

        void reset() { x1 = x2 = y1 = y2 = 0; }

        float process(float x)
        {
            float y = a2 * (x + y2) - x2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }
    };

    struct Delay1 {
        float z1 = 0;
        void reset() { z1 = 0; }
        float process(float x) { float y = z1; z1 = x; return y; }
    };

    AllpassZ2 mAP1[4];
    AllpassZ2 mAP2[4];
    Delay1    mDelay1;

    float mScale = (float)1;
    float mLastI = 0;
    float mLastQ = 0;
};
