#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

class AdditiveResynth
{
public:
    AdditiveResynth(unsigned fftSize,
                    unsigned hopSize,
                    float sampleRate)
    : mFFTSize(fftSize),
      mHopSize(hopSize),
      mSampleRate(sampleRate)
    {
        const unsigned bins = fftSize / 2 + 1;
        mOsc.resize(bins);

        // amplitude smoothing (optional but recommended)
        mAmpSmooth.resize(bins, 0.0f);
    }

    // ------------------------------------------------------------
    // Controls
    // ------------------------------------------------------------

    void setAmpScale(float g)      { mAmpScale = g; }
    void setMagThreshold(float t)  { mMagThreshold = std::max(0.f, t); }
    void setSmoothing(float a)     { mSmooth = std::clamp(a, 0.f, 1.f); }

    // Call this on detected transients
    void resetPhase()
    {
        for (auto& o : mOsc)
            o.setPhase(0.0f);
    }

    // ------------------------------------------------------------
    // Analysis → Oscillator update (call once per STFT frame)
    // ------------------------------------------------------------

    void analyzeFrame(const float* magFreqInterleaved)
    {
        const unsigned bins = mOsc.size();

        for (unsigned k = 1; k + 1 < bins; ++k)
        {
            const float mag  = magFreqInterleaved[2 * k + 0];
            const float freq = magFreqInterleaved[2 * k + 1];

            if (mag < mMagThreshold || freq <= 0.0f)
            {
                mAmpSmooth[k] = 0.0f;
                mOsc[k].setAmp(0.0f);
                continue;
            }

            // smooth amplitude to avoid zipper noise
            float targetAmp = mag * mAmpScale;
            mAmpSmooth[k] = (1.0f - mSmooth) * targetAmp
                            + mSmooth * mAmpSmooth[k];

            mOsc[k].setFreq(freq);
            mOsc[k].setAmp(mAmpSmooth[k]);
        }
    }

    // ------------------------------------------------------------
    // Synthesis (call per sample or per block)
    // ------------------------------------------------------------

    inline float process()
    {
        float y = 0.0f;
        for (unsigned k = 1; k + 1 < mOsc.size(); ++k)
            y += mOsc[k].process();
        return y;
    }

    void run(float* output, unsigned n)
    {
        for (unsigned i = 0; i < n; ++i)
            output[i] = process();
    }

    // ------------------------------------------------------------
    // Utilities
    // ------------------------------------------------------------

    unsigned numBins() const { return mOsc.size(); }

private:
    unsigned mFFTSize;
    unsigned mHopSize;
    float    mSampleRate;

    std::vector<CSine> mOsc;
    std::vector<float> mAmpSmooth;

    float mAmpScale     = 1.0f;
    float mMagThreshold = 1e-5f;
    float mSmooth       = 0.9f;  // closer to 1 = smoother
};
