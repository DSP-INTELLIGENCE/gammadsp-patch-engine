#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class AdaptiveController : public Td {
public:
    AdaptiveController(T timeConstant = T(0.05)){
        responseTime(timeConstant);
        reset();
    }

    void reset(T v = T(0)){
        mValue = v;
    }

    /// Time (seconds) to reach ~63% of target
    /// Call outside audio thread
    void responseTime(T seconds){
        mTime = scl::max(seconds, T(1e-6));
        updateCoeff();
    }

    /// Desired control target
    void target(T t){ mTarget = t; }

    /// State machine weighting [0..1]
    void stateWeight(T w){
        mStateWeight = scl::clip(w, T(0), T(1)); // FIXED
    }

    /// Per-sample update
    T operator()(){
        T goal = mTarget * mStateWeight;
        mValue += mAlpha * (goal - mValue);
        return mValue;
    }

    T value() const { return mValue; }

    /// Domain change hook (sample-rate invariant)
    void onDomainChange(double){
        updateCoeff();
    }

private:
    void updateCoeff(){
        // α = 1 − exp(−1 / (τ·Fs))
        mAlpha = T(1) - scl::exp(T(-1) / (mTime * Td::spu()));
    }

    T mTarget      = T(0);
    T mValue       = T(0);
    T mStateWeight = T(1);

    T mTime  = T(0.05);
    T mAlpha = T(0.05);
};


} // namespace gam
