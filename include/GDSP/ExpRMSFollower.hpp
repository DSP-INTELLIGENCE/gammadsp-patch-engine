// ExpRMSFollower.hpp
#pragma once
#include <algorithm>
#include <cmath>

// Exponential RMS follower (IIR energy integrator).
// - No allocations
// - float-rate independent
// - Attack/Release on ENERGY (x^2), then sqrt to get RMS
// - Optional dB readout helper

class ExpRMSFollower {
public:
    explicit ExpRMSFollower(float attackSeconds  = 0.010f,
                            float releaseSeconds = 0.100f)
    : mAttackSec(std::max(1e-5f, attackSeconds))
    , mReleaseSec(std::max(1e-5f, releaseSeconds))
    , mAttackCoeff(0.0f)
    , mReleaseCoeff(0.0f)
    , mEnergy(0.0f)
    , mRMS(0.0f)
    {
        updateCoeffs();
    }
    
    // Time constants (seconds)
    void setAttack(float seconds)  { mAttackSec  = std::max(1e-5f, seconds); updateCoeffs(); }
    void setRelease(float seconds) { mReleaseSec = std::max(1e-5f, seconds); updateCoeffs(); }

    float attack()  const { return mAttackSec; }
    float release() const { return mReleaseSec; }

    // Reset state (energy is mean-square; e.g. RMS=0.1 -> energy=0.01)
    void reset(float initialRMS = 0.0f) {
        float r = std::max(0.0f, initialRMS);
        mEnergy = r * r;
        mRMS    = r;
    }

    // Process one sample, return RMS (linear)
    float process(float x) {
        // Energy input
        const float e = x * x;

        // Choose coefficient based on whether energy is rising or falling
        const float coeff = (e > mEnergy) ? mAttackCoeff : mReleaseCoeff;

        // One-pole smoothing on energy
        mEnergy = (1.0f - coeff) * e + coeff * mEnergy;

        // Denormal + zero protection before sqrt
        mEnergy = std::max(mEnergy, kEnergyFloor);

        mRMS = std::sqrt(mEnergy);
        return mRMS;
    }

    float value() const { return mRMS; }      // RMS (linear)
    float energy() const { return mEnergy; }  // Mean-square

    // RMS in dBFS (helper). For audio normalized to [-1,1], 0 dBFS == RMS 1.0.
    float valueDb(float floorDb = -120.0f) const {
        // 10*log10(energy) == 20*log10(rms)
        float db = 10.0f * std::log10(std::max(mEnergy, kEnergyFloor));
        return std::max(db, floorDb);
    }

private:
    void updateCoeffs() {
        // Standard one-pole coefficient from time constant:
        // coeff = exp(-1/(tau*Fs))
        mAttackCoeff  = std::exp(-1.0f / (mAttackSec  * gam::floatRate()));
        mReleaseCoeff = std::exp(-1.0f / (mReleaseSec * gam::floatRate()));

        // Safety clamp (paranoia)
        mAttackCoeff  = std::clamp(mAttackCoeff,  0.0f, 0.9999999f);
        mReleaseCoeff = std::clamp(mReleaseCoeff, 0.0f, 0.9999999f);
    }

    static constexpr float kEnergyFloor = 1e-12f;

    float mAttackSec;
    float mReleaseSec;

    float mAttackCoeff;
    float mReleaseCoeff;

    float mEnergy; // mean-square envelope
    float mRMS;    // sqrt(mean-square)
};
