#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class AdaptiveControllerAR : public Td {
public:
    AdaptiveControllerAR(T attackTime  = T(0.02),
                         T releaseTime = T(0.10))
    {
        attack(attackTime);
        release(releaseTime);
        reset();
    }

    void reset(T v = T(0)){
        mValue = v;
    }

    /// Time (seconds) to reach ~63% when rising
    void attack(T seconds){
        mAttackTime = scl::max(seconds, T(1e-6));
        updateCoeffs();
    }

    /// Time (seconds) to reach ~63% when falling
    void release(T seconds){
        mReleaseTime = scl::max(seconds, T(1e-6));
        updateCoeffs();
    }

    /// Desired control target
    void target(T t){
        mTarget = t;
    }

    /// State weight [0..1]
    void stateWeight(T w){
        mStateWeight = scl::clip(w, T(0), T(1));
    }

    /// Per-sample update
    T operator()(){
        const T goal = mTarget * mStateWeight;
        const T alpha = (goal > mValue) ? mAlphaAttack : mAlphaRelease;
        mValue += alpha * (goal - mValue);
        return mValue;
    }

    T value() const { return mValue; }

    /// Domain change hook (sample-rate invariant)
    void onDomainChange(double){
        updateCoeffs();
    }

private:
    void updateCoeffs(){
        // α = 1 − exp(−1 / (τ·Fs))
        const T invFs = Td::spu();
        mAlphaAttack  = T(1) - scl::exp(T(-1) / (mAttackTime  * invFs));
        mAlphaRelease = T(1) - scl::exp(T(-1) / (mReleaseTime * invFs));
    }

    // ---- state ----
    T mTarget      = T(0);
    T mValue       = T(0);
    T mStateWeight = T(1);

    // ---- timing ----
    T mAttackTime  = T(0.02);
    T mReleaseTime = T(0.10);

    // ---- coefficients ----
    T mAlphaAttack  = T(0.1);
    T mAlphaRelease = T(0.01);
};

} // namespace gam
