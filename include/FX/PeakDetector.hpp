#pragma once
#include <algorithm>
#include <cmath>

// High-quality peak detector with independent attack & release

class PeakDetector {
public:
    explicit PeakDetector(float attackSeconds  = 0.001f,
                          float releaseSeconds = 0.050f)
    : mAttackSec(std::max(1e-6f, attackSeconds))
    , mReleaseSec(std::max(1e-6f, releaseSeconds))
    , mAttackCoeff(0.0f)
    , mReleaseCoeff(0.0f)
    , mValue(0.0f)
    {
        updateCoeffs();
    }
    
    void setAttack(float seconds) {
        mAttackSec = std::max(1e-6f, seconds);
        updateCoeffs();
    }

    void setRelease(float seconds) {
        mReleaseSec = std::max(1e-6f, seconds);
        updateCoeffs();
    }

    float attack() const  { return mAttackSec; }
    float release() const { return mReleaseSec; }

    void reset(float value = 0.0f) {
        mValue = std::max(0.0f, value);
    }

    // Per-sample processing
    float process(float x) {
        float input = std::abs(x);
        float coeff = (input > mValue) ? mAttackCoeff : mReleaseCoeff;

        mValue = (1.0f - coeff) * input + coeff * mValue;

        // Denormal protection
        if (mValue < kFloor) mValue = 0.0f;

        return mValue;
    }

    float value() const { return mValue; }

    // Peak in dBFS
    float valueDb(float floorDb = -120.0f) const {
        float db = 20.0f * std::log10(std::max(mValue, kFloor));
        return std::max(db, floorDb);
    }

private:
    void updateCoeffs() {
        mAttackCoeff  = std::exp(-1.0f / (mAttackSec  * gam::sampleRate()));
        mReleaseCoeff = std::exp(-1.0f / (mReleaseSec * gam::sampleRate()));

        mAttackCoeff  = std::clamp(mAttackCoeff,  0.0f, 0.9999999f);
        mReleaseCoeff = std::clamp(mReleaseCoeff, 0.0f, 0.9999999f);
    }

    static constexpr float kFloor = 1e-12f;

    
    float mAttackSec;
    float mReleaseSec;
    float mAttackCoeff;
    float mReleaseCoeff;
    float mValue;
};
