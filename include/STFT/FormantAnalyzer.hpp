#pragma once
#include <algorithm>
#include <cmath>
#include <vector>


class FormantAnalyzer {
public:
    struct Result {
        float F1 = 0;
        float F2 = 0;
        float energy = 0;
    };

    explicit FormantAnalyzer(unsigned order = 10)
    : mOrder(order)
    {
        reset();
    }

    void reset()
    {
        std::fill(mHistory.begin(), mHistory.end(), (float)0);
        mIndex = 0;
        mResult = {};
    }

    Result process(float x)
    {
        mHistory[mIndex] = x;
        mIndex = (mIndex + 1) % mWindow;

        if (++mCounter < mWindow)
            return mResult;

        mCounter = 0;
        analyze();
        return mResult;
    }

    Result value() const { return mResult; }

private:
    void analyze()
    {
        // Compute autocorrelation
        for (unsigned i = 0; i <= mOrder; ++i)
        {
            mR[i] = 0;
            for (unsigned j = 0; j < mWindow; ++j)
            {
                unsigned k = (j + i) % mWindow;
                mR[i] += mHistory[j] * mHistory[k];
            }
        }

        // Levinson–Durbin recursion
        std::vector<float> a(mOrder + 1, 0);
        std::vector<float> e(mOrder + 1, 0);
        e[0] = mR[0];

        for (unsigned i = 1; i <= mOrder; ++i)
        {
            float acc = mR[i];
            for (unsigned j = 1; j < i; ++j)
                acc += a[j] * mR[i - j];

            float k = -acc / (e[i - 1] + (float)1e-9);
            a[i] = k;

            for (unsigned j = 1; j < i; ++j)
                a[j] = a[j] + k * a[i - j];

            e[i] = (float)(1 - k * k) * e[i - 1];
        }

        // Find roots → formants (simple spectral peak search)
        findFormants(a);
    }

    void findFormants(const std::vector<float>& a)
    {
        float prev = 0;
        float f1 = 0, f2 = 0;

        for (unsigned i = 1; i < mOrder; ++i)
        {
            float mag = std::abs(a[i]);
            if (mag > prev && mag > (float)0.05)
            {
                float freq = (float)gam::sampleRate() * (float)i / (float)mOrder;
                if (f1 == 0) f1 = freq;
                else if (f2 == 0) { f2 = freq; break; }
            }
            prev = mag;
        }

        mResult.F1 = f1;
        mResult.F2 = f2;
        mResult.energy = mR[0];
    }

private:
    static constexpr unsigned mWindow = 256;

    unsigned mOrder;
    unsigned mIndex = 0;
    unsigned mCounter = 0;

    float mHistory[mWindow] {};
    float mR[32] {};

    Result mResult;
};
