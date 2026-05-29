#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

class EchoAnalyzer {
public:
    EchoAnalyzer(float maxDelaySeconds,
                 int   numTaps,
                 float minDelaySeconds,
                 float maxDelayTapSeconds)
    : mDelay(maxDelaySeconds, numTaps),
      mEnv(numTaps),
      mTap(numTaps, 0.0f),
      mDecay(numTaps, 1.0f)
    {
        // logarithmic tap spacing (psychoacoustically correct)
        for (int i = 0; i < numTaps; ++i)
        {
            float t = float(i) / float(numTaps - 1);
            float d = minDelaySeconds *
                      std::pow(maxDelayTapSeconds / minDelaySeconds, t);

            mDelay.setTapTime(i, d);
        }

        setEnvelopeTime(20.0f);   // default smoothing
    }

    // --------------------------------------------------

    void process(float x)
    {
        mDelay.write(x);

        float totalEnergy = 0.0f;

        for (int i = 0; i < size(); ++i)
        {
            float v = mDelay.read(i);
            mTap[i] = v;

            float e = mEnv[i].process(std::fabs(v));
            totalEnergy += e;
        }

        // optional normalization
        if (normalize && totalEnergy > 1e-9f)
        {
            for (int i = 0; i < size(); ++i)
                mEnv[i].setValue(mEnv[i].value() / totalEnergy);

            totalEnergy = 1.0f;
        }

        // decay ratios (temporal slope)
        for (int i = 1; i < size(); ++i)
        {
            float prev = mEnv[i - 1].value();
            float cur  = mEnv[i].value();
            mDecay[i]  = (prev > 1e-6f) ? cur / prev : 0.0f;
        }
        mDecay[0] = 1.0f;

        mEnergy = totalEnergy;
    }

    // --------------------------------------------------
    // Accessors

    float tap(int i)   const { return mTap[i]; }
    float env(int i)   const { return mEnv[i].value(); }
    float decay(int i) const { return mDecay[i]; }

    float energy() const { return mEnergy; }
    int   size()   const { return (int)mTap.size(); }

    // --------------------------------------------------
    // Configuration

    void setTapDelay(int i, float seconds)
    {
        mDelay.setTapTime(i, seconds);
    }

    void setEnvelopeTime(float ms)
    {
        for (auto& e : mEnv)
            e.setTime(ms);
    }

    void setNormalize(bool on)
    {
        normalize = on;
    }

private:
    MultitapDelay          mDelay;
    std::vector<EnvFollow> mEnv;
    std::vector<float>    mTap;
    std::vector<float>    mDecay;

    float mEnergy = 0.0f;
    bool  normalize = true;
};
