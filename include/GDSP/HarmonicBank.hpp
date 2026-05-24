#pragma once
#include <Gamma/DFT.h>
#include <vector>
#include <algorithm>
#include <cmath>

class HarmonicBank
{
public:
    HarmonicBank(unsigned windowSize = 1024,
                        unsigned hopSize    = 256,
                        gam::WindowType winType = gam::HANN)
    : mSTFT(windowSize, hopSize, 0, winType, gam::MAG_FREQ),
      mOscBank(mSTFT.numBins())
    {
        // No inverse windowing — we are not using IFFT
        mSTFT.inverseWindowing(false);

        mMag.resize(mSTFT.numBins());
        mFreq.resize(mSTFT.numBins());
    }

    // Optional amplitude trim
    void setGain(float g) { mGain = g; }

    // -------- Analysis + control update --------
    // Feed samples one-by-one
    bool process(float input)
    {
        if (mSTFT(input))
        {
            const unsigned bins = mSTFT.numBins();

            for (unsigned k = 1; k + 1 < bins; ++k)
            {
                float mag  = mSTFT.bin(k)[0];
                float freq = mSTFT.bin(k)[1];

                // Optional magnitude conditioning
                mag = std::max(0.0f, mag);

                // Drive oscillator (NO phase reset)
                mOscBank.set(k, freq, mag);
            }

            return true; // new frame applied
        }
        return false;
    }

    // -------- Synthesis --------
    float synth()
    {
        return mOscBank.process() * mGain;
    }

    // Block version
    void run(const float* input,
             float* output,
             size_t n)
    {
        for (size_t i = 0; i < n; ++i)
        {
            process(input[i]);
            output[i] = synth();
        }
    }

    unsigned numPartials() const
    {
        return mOscBank.size();
    }

private:
    gam::STFT mSTFT;
    SineRs    mOscBank;

    std::vector<float> mMag;
    std::vector<float> mFreq;

    float mGain = 1.0f;
};
