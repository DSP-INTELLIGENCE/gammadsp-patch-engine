#pragma once
#include <algorithm>
#include <cmath>

// Transient detector using energy derivative & adaptive normalization
template <class Sample>
class TransientDetector {
public:
    TransientDetector(Sample attackSec  = 0.001f,
                      Sample releaseSec = 0.050f)
    : mAttackSec(std::max((Sample)1e-6, attackSec)),
      mReleaseSec(std::max((Sample)1e-6, releaseSec))
    {
        updateCoeffs();
    }

    void setAttack(Sample sec)  { mAttackSec  = std::max((Sample)1e-6, sec); updateCoeffs(); }
    void setRelease(Sample sec) { mReleaseSec = std::max((Sample)1e-6, sec); updateCoeffs(); }

    // Returns transient strength in [0, 1]
    Sample process(Sample x)
    {
        // Energy
        Sample e = x * x;

        // Fast energy follower
        mFastEnergy = (Sample)0.1 * e + (Sample)0.9 * mFastEnergy;

        // Slow energy follower
        mSlowEnergy = (Sample)0.001 * e + (Sample)0.999 * mSlowEnergy;

        // Energy difference = attack strength
        Sample diff = mFastEnergy - mSlowEnergy;
        diff = std::max((Sample)0, diff);

        // Adaptive normalization
        mTransientEnv = (diff > mTransientEnv)
                        ? mAttackCoeff  * mTransientEnv + (Sample)(1 - mAttackCoeff)  * diff
                        : mReleaseCoeff * mTransientEnv + (Sample)(1 - mReleaseCoeff) * diff;

        Sample out = diff / (mTransientEnv + (Sample)1e-12);

        // Clamp & denormal protect
        out = std::clamp(out, (Sample)0, (Sample)1);
        if (out < (Sample)1e-20) out = 0;

        return out;
    }

private:
    void updateCoeffs()
    {
        const Sample fs = (Sample)gam::sampleRate();
        mAttackCoeff  = std::exp(-(Sample)1 / (mAttackSec  * fs));
        mReleaseCoeff = std::exp(-(Sample)1 / (mReleaseSec * fs));
    }

    Sample mAttackSec, mReleaseSec;
    Sample mAttackCoeff = 0, mReleaseCoeff = 0;

    Sample mFastEnergy = 0;
    Sample mSlowEnergy = 0;
    Sample mTransientEnv = 0;
};
