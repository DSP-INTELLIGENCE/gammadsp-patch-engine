#pragma once
#include <array>
#include <cmath>
#include <algorithm>
#include <complex>

template<class Sample, int LPC_ORDER = 12>
class LPCFormantAnalyzer {
public:
    struct Result {
        Sample F1 = 0;
        Sample F2 = 0;
        Sample energy = 0;
    };

    explicit LPCFormantAnalyzer(Sample sampleRate)
    : mSR(sampleRate)
    {
        reset();
    }

    void reset() {
        mPos = 0;
        mReady = false;
        std::fill(mFrame.begin(), mFrame.end(), Sample(0));
    }

    Result process(Sample x)
    {
        mFrame[mPos++] = x;

        if (mPos >= WINDOW) {
            mPos = 0;
            analyze();
            mReady = true;
        }

        return mResult;
    }

    Result value() const { return mResult; }

private:
    static constexpr int WINDOW = 512;

    Sample mSR;
    int mPos = 0;
    bool mReady = false;

    std::array<Sample, WINDOW> mFrame {};
    std::array<Sample, LPC_ORDER+1> mR {};
    std::array<Sample, LPC_ORDER+1> mA {};
    std::array<Sample, LPC_ORDER+1> mPrevA {};

    Result mResult;

    // ---------- Analysis Pipeline ----------

    void analyze()
    {
        applyWindow();
        autocorrelate();
        levinsonDurbin();
        extractFormants();
    }

    void applyWindow()
    {
        for(int i=0;i<WINDOW;i++)
            mFrame[i] *= Sample(0.54) - Sample(0.46) * std::cos(Sample(2*M_PI) * i / (WINDOW-1));
    }

    void autocorrelate()
    {
        for(int i=0;i<=LPC_ORDER;i++){
            Sample sum = 0;
            for(int j=0;j<WINDOW-i;j++)
                sum += mFrame[j] * mFrame[j+i];
            mR[i] = sum;
        }
        mResult.energy = mR[0];
    }

    void levinsonDurbin()
    {
        std::fill(mA.begin(), mA.end(), Sample(0));
        std::fill(mPrevA.begin(), mPrevA.end(), Sample(0));

        Sample err = mR[0] + Sample(1e-9);

        for(int i=1;i<=LPC_ORDER;i++){
            Sample acc = mR[i];
            for(int j=1;j<i;j++)
                acc += mPrevA[j] * mR[i-j];

            Sample k = -acc / err;

            mA[i] = k;
            for(int j=1;j<i;j++)
                mA[j] = mPrevA[j] + k * mPrevA[i-j];

            err *= Sample(1) - k*k;
            mPrevA = mA;
        }
    }

    void extractFormants()
    {
        constexpr int MAXP = LPC_ORDER;

        std::array<Sample, MAXP+1> poly {};
        poly[0] = Sample(1);
        for(int i=1;i<=LPC_ORDER;i++)
            poly[i] = mA[i];

        auto roots = lpc::find_roots_real_poly<Sample, MAXP>(poly, LPC_ORDER);

        Sample f1 = 0, f2 = 0;
        for(int i=0;i<roots.n;i++){
            auto z = roots.roots[i];
            if(z.imag() <= 0) continue;

            Sample mag = std::abs(z);
            if(mag >= Sample(1)) continue;

            Sample freq = std::atan2(z.imag(), z.real()) * mSR / (Sample(2*M_PI));
            if(freq > Sample(80) && freq < Sample(5000)) {
                if(!f1) f1 = freq;
                else if(!f2) { f2 = freq; break; }
            }
        }

        mResult.F1 = f1;
        mResult.F2 = f2;
    }
};
