#pragma once
#include <Gamma/DFT.h>
#include <vector>
#include <algorithm>
#include <cmath>

class OnsetDetector {
public:
    OnsetDetector(unsigned windowSize = 1024,
                  unsigned hopSize    = 256,
                  gam::WindowType winType = gam::HANN)
    : mSTFT(windowSize, hopSize, 0, winType, gam::MAG, 1),
      mHop(hopSize)
    {
        const unsigned bins = mSTFT.numBins();
        mPrevMag.assign(bins, 0.0f);
        mCurrMag.assign(bins, 0.0f);
    }

    void setSensitivity(float s) { mSensitivity = std::clamp(s, 0.f, 1.f); }
    bool  isOnset() const        { return mOnset; }
    float strength() const      { return mStrength; }

    // Feed one analysis window
    void process(const float* window)
    {
        mOnset = false;

        mSTFT.forward(window);

        const unsigned bins = mSTFT.numBins();

        // Copy magnitudes
        for (unsigned k = 0; k < bins; ++k)
            mCurrMag[k] = mSTFT.bin(k)[0];

        analyze();

        std::swap(mPrevMag, mCurrMag);
    }

private:
    gam::STFT mSTFT;
    unsigned  mHop;

    std::vector<float> mPrevMag;
    std::vector<float> mCurrMag;

    float mFlux = 0.0f;
    float mThreshold = 0.0f;
    float mStrength = 0.0f;
    bool  mOnset = false;

    float mSensitivity = 0.5f;

    void analyze()
    {
        // --- Spectral Flux ---
        float flux = 0.0f;
        for (unsigned k = 0; k < mCurrMag.size(); ++k)
        {
            float diff = mCurrMag[k] - mPrevMag[k];
            if (diff > 0.0f)
                flux += diff;
        }

        // --- Adaptive threshold ---
        mThreshold = 0.9f * mThreshold + 0.1f * flux;

        // --- Detection ---
        const float adaptive = mThreshold * (1.0f + mSensitivity);

        mOnset    = (flux > adaptive);
        mStrength = flux;
    }
};
