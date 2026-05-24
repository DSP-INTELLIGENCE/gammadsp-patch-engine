#pragma once
#include <algorithm>
#include <cmath>
#include "Comb.hpp"
#include "Parameters.hpp"

class CombFilter : public Function
{
public:
    enum Mode { FEEDFORWARD, FEEDBACK, ALLPASS, PHYSICAL };

    CombFilter(float maxDelaySec = 2.0f,
               float initFreqHz = 440.0f,
               float initRes01  = 0.25f,
               Mode  mode = FEEDBACK)
    : mComb(maxDelaySec, 1.0f / std::max(20.0f, initFreqHz))
    {
        mMaxDelay = std::max(0.001f, maxDelaySec);
        baseCutoff = initFreqHz;
        baseRes    = std::clamp(initRes01, 0.0f, 1.0f);

        setMode(mode);
        setAM(1.0f);
    }

    // ----- Filter-like API -----
    void setFreq(float hz) { baseCutoff = hz; }
    void setRes (float r)  { baseRes = std::clamp(r, 0.f, 1.f); }
    void setWet (float w)  { mWet = std::clamp(w, 0.f, 1.f); }
    void setFM(float v)    { cutoff.set(v);}
    void setQM(float v)    { res.set(v); }
    void setAM(float v)    { am.set(v);}

    void setMode(Mode m)
    {
        mMode = m;
        mFbMax = mFfMax = mApMax = 0.0f;

        if (m == FEEDBACK || m == PHYSICAL) mFbMax = 0.995f;
        if (m == FEEDFORWARD)               mFfMax = 0.95f;
        if (m == ALLPASS)                   mApMax = 0.95f;
    }

    void setDamp(float d) { mDampTarget = std::clamp(d, 0.f, 1.f); }
    void setInvertFeedback(bool v) { mInvertFb = v; }

    float process(float x) override
    {
        // ---- modulation ----
        float fMod = cutoff.process();
        float rMod = res.process();
        float aMod = am.process();

        float freq = baseCutoff * (1.0f + fMod);
        float reso = baseRes    * (1.0f + rMod);

        freq = std::clamp(freq, 20.0f, 1.0f / mMinDelay);
        reso = std::clamp(reso, 0.0f, 1.0f);

        updateTargets(freq, reso);

        // ---- apply params ----
        mComb.setDelay(mDelay);
        mComb.setFeedback(mFb);
        mComb.setFeedforward(mFf);
        mComb.setAllPass(mAp);

        float y = 0.0f;

        if (mMode == PHYSICAL)
        {
            float d = mComb.read();
            float fb = mInvertFb ? -mFb : mFb;

            float a = 0.001f + (1.0f - mDampTarget) * 0.2f;
            mDampZ += a * (d - mDampZ);

            mComb.write(x + fb * mDampZ);
            y = d;
        }
        else
        {
            y = mComb.process(x);
        }

        float out = (1.0f - mWet) * x + mWet * y;
        return out * aMod;
    }

    Modulator cutoff, res, am;

private:
    void updateTargets(float freq, float res01)
    {
        float delay = 1.0f / std::max(freq, 1e-6f);
        delay = std::clamp(delay, mMinDelay, mMaxDelay);

        constexpr float k = 6.0f;
        float shaped = 1.0f - std::exp(-k * res01);

        mFb = shaped * mFbMax;
        mFf = shaped * mFfMax;
        mAp = shaped * mApMax;
        ///mDamp = mDampTarget;

        mFb = std::clamp(mFb, -0.999f, 0.999f);
        mFf = std::clamp(mFf, -0.999f, 0.999f);
        mAp = std::clamp(mAp, -0.999f, 0.999f);
        mDelay = delay;
    }

private:
    Comb  mComb;    
    Mode  mMode = FEEDBACK;

    float baseCutoff;
    float baseRes;

    float mWet = 1.0f;
    bool  mInvertFb = false;

    float mMaxDelay = 2.0f;
    float mMinDelay = 1.0f / 8000.0f;

    float mFbMax = 0.0f;
    float mFfMax = 0.0f;
    float mApMax = 0.0f;

    float mDelay = 0.01f;
    float mFb = 0.0f;
    float mFf = 0.0f;
    float mAp = 0.0f;
    float mDampTarget = 0.5f;
    float mDampZ = 0.0f;
};
