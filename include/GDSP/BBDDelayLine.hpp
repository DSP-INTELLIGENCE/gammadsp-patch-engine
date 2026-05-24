#pragma once
#include <vector>
#include <cmath>
#include <algorithm>


#include "Engine.hpp"
#include "Parameters.hpp"
#include "SoftClipper.hpp"
#include "OnePole.hpp"
#include "Noise.hpp"

class Compander
{
public:
    void setAmount(float a) { mAmount = std::clamp(a, 0.f, 1.f); }
    void reset() { mEnv = 0.0f; }

    float processIn(float x);   // compressor
    float processOut(float x);  // expander

private:
    float mEnv = 0.0f;
    float mAmount = 1.0f;
};

class BBDDelayLine
{
public:
    BBDDelayLine(size_t stages = 2048, float sampleRate = 48000.0f);

    void setDelay(float seconds);
    void setFeedback(float v);
    void setTone(float v);
    void setDrive(float v);
    void setDepth(float v);
    void setRate(float hz);
    void setBlend(float v);
    void setAge(float v);

    float process(float input);
    void reset();

private:
    void updateClock();
    
    float clockToCutoff() const;

private:
    // Transport (two-phase banks)
    std::vector<float> mBufA;
    std::vector<float> mBufB;
    size_t mStageCount = 0;
    float mWritePos = 0.0f;     // stage index (wraps)
    bool  mPhi = false;         // false=phi1, true=phi2

    // Clock feedthrough state
    float mClkSquare = 1.0f;    // toggles each half-tick

    // Clock
    float mPhase = 0.0f;
    float mClockHz = 30000.0f;
    float mClockBase = 30000.0f;
    float mSampleRate;

    // Parameters
    float mDelay = 0.4f;
    float mFeedback = 0.5f;
    float mTone = 0.5f;
    float mDrive = 0.3f;
    float mDepth = 0.0f;
    float mRate = 0.25f;
    float mBlend = 0.5f;
    float mAge = 0.3f;

    // Modulation
    Modulator mLFO;

    // Filters & color
    OnePole mPreLPF, mPostLPF, mFB_LPF;
    OnePole mPreLPF1, mPreLPF2;
    OnePole mPostLPF1, mPostLPF2;

    SoftClipper mClip;
    NoiseWhite mNoise;
    Compander mCompander;

    // State
    float mLastWet = 0.0f;

    inline float cubicInterp(float a, float b, float c, float d, float t) const;
    void halfTick(float in);                // performs phi1/phi2 actions
    float readCubic(const std::vector<float>& buf, float pos) const;
    float clockFeedAmount() const;


};
