#pragma once
#include <algorithm>
#include <cmath>

#include "EnvFollow.hpp"
#include "RMSFollower.hpp"


class EnergyDynamics {
public:
    EnergyDynamics()
    : mEnv((float)30),
      mRMS((float)0.05),
      mBlend((float)0.5),
      mTarget((float)0.15),
      mRatio((float)3),
      mKnee((float)0.08),
      mMaxBoost((float)4),
      mMaxCut((float)0.05),
      mAttack((float)0.010),
      mRelease((float)0.120),
      mMakeup((float)1),
      mEnergy((float)0),
      mGain((float)1)
    {}

    // --- Analyzer controls ---
    void setDetectorBlend(float b) { mBlend = std::clamp(b, (float)0, (float)1); }
    void setEnvFreq(float hz)      { mEnv.setFreq(hz); }
    void setRmsWindow(float sec)   { mRMS.setWindow(sec); }

    // --- Dynamics goals ---
    void setTarget(float t)    { mTarget = std::max((float)1e-6, t); }
    void setRatio(float r)     { mRatio = std::max((float)1, r); }
    void setKnee(float k)      { mKnee = std::max((float)0, k); }
    void setMaxBoost(float g)  { mMaxBoost = std::max((float)1, g); }
    void setMaxCut(float g)    { mMaxCut = std::clamp(g, (float)1e-6, (float)1); }

    // --- Smoothing ---
    void setAttack(float sec)  { mAttack = std::max((float)1e-6, sec); }
    void setRelease(float sec) { mRelease = std::max((float)1e-6, sec); }

    // --- Optional trim ---
    void setMakeup(float g) { mMakeup = g; }

    void reset()
    {
        mEnv.reset();
        mRMS.reset();
        mEnergy = (float)0;
        mGain   = (float)1;
    }

    // One call: analyze + apply dynamics
    float process(float x)
    {
        float env = mEnv.process(std::abs(x));
        float rms = mRMS.process(x);

        mEnergy = ((float)1 - mBlend) * env + mBlend * rms;

        float targetGain = computeTargetGain(mEnergy);

        // Smooth gain: attack when reducing, release when increasing
        float c = (targetGain < mGain) ? timeCoeff(mAttack) : timeCoeff(mRelease);
        mGain = targetGain + c * (mGain - targetGain);

        return x * mGain * mMakeup;
    }

    // --- Telemetry ---
    float energy() const { return mEnergy; }
    float gain()   const { return mGain; }

    float gainDb() const
    {
        float g = std::max((float)1e-9, mGain);
        return (float)20 * std::log10(g);
    }

private:
    float timeCoeff(float sec) const
    {
        sec = std::max((float)1e-6, sec);
        return std::exp((float)-1 / (sec * (float)gam::sampleRate()));
    }

    float computeTargetGain(float level) const
    {
        float t = mTarget;
        float r = mRatio;
        float k = mKnee;

        level = std::max((float)1e-9, level);

        auto gainAbove = [&](float L) {
            float outL = t + (L - t) / r;
            return outL / L;
        };

        auto gainBelow = [&](float L) {
            float x = L / t;
            float outL = t * std::pow(x, (float)1 / r);
            return outL / L;
        };

        float g;

        if (k <= (float)0) {
            g = (level >= t) ? gainAbove(level) : gainBelow(level);
        } else {
            float halfK = (float)0.5 * k;
            float lo = t - halfK;
            float hi = t + halfK;

            if      (level <= lo) g = gainBelow(level);
            else if (level >= hi) g = gainAbove(level);
            else {
                float u = (level - lo) / (hi - lo);
                float s = u * u * ((float)3 - (float)2 * u);

                float gLo = gainBelow(level);
                float gHi = gainAbove(level);

                g = ((float)1 - s) * gLo + s * gHi;
            }
        }

        g = std::min(g, mMaxBoost);
        g = std::max(g, mMaxCut);
        return g;
    }

private:
    EnvFollow   mEnv;
    RMSFollower mRMS;

    float mBlend;
    float mTarget;
    float mRatio;
    float mKnee;
    float mMaxBoost;
    float mMaxCut;
    float mAttack;
    float mRelease;
    float mMakeup;

    float mEnergy;
    float mGain;
};
