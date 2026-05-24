#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ZDFCrossoverLR4 : public Td {
public:
    ZDFCrossoverLR4(){
        smoothTime(T(0.01));     // 10 ms default smoothing
        cutoff(T(1000));
        reset();
    }

    // ------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------
    void reset(){
        lp1.reset(); lp2.reset();
        hp1.reset(); hp2.reset();

        mFcZ = mFcTarget;
        updateSVFParams();
    }

    void onDomainChange(double){
        updateAlpha();
        updateSVFParams();
    }

    // ------------------------------------------------------------
    // Parameters (safe to modulate)
    // ------------------------------------------------------------
    void cutoff(T hz){
        mFcTarget = std::max(hz, T(0));
    }

    /// Smoothing time constant (seconds, ~63%)
    /// Set to 0 for immediate response (still ZDF-stable).
    void smoothTime(T seconds){
        mSmoothTime = std::max(seconds, T(0));
        updateAlpha();
    }

    // ------------------------------------------------------------
    // DSP
    // ------------------------------------------------------------
    inline void operator()(T x, T& low, T& high){
        tickSmoother();
        updateSVFParams();   // 🔥 per-sample coefficient update

        // ----- low path -----
        lp1.process(x);
        lp2.process(lp1.low());
        low = lp2.low();

        // ----- high path -----
        hp1.process(x);
        hp2.process(hp1.high());
        high = hp2.high();
    }

    /// Flat-sum convenience
    inline T sum(T x){
        T l, h;
        (*this)(x, l, h);
        return l + h;
    }

private:
    // ------------------------------------------------------------
    // Per-sample smoother
    // ------------------------------------------------------------
    inline void tickSmoother(){
        if(mAlpha <= T(0)){
            mFcZ = mFcTarget;
            return;
        }
        mFcZ += mAlpha * (mFcTarget - mFcZ);
    }

    // ------------------------------------------------------------
    // SVF parameter push
    // ------------------------------------------------------------
    inline void updateSVFParams(){
        const T fs = T(Td::spu());
        const T fc = scl::clip(mFcZ, T(1e-4), T(0.49) * fs);

        // LR4 = Butterworth cascades
        constexpr T Q = T(0.7071067811865476);

        lp1.freq(fc); lp1.res(Q);
        lp2.freq(fc); lp2.res(Q);

        hp1.freq(fc); hp1.res(Q);
        hp2.freq(fc); hp2.res(Q);
    }

    inline void updateAlpha(){
        if(mSmoothTime <= T(0)){
            mAlpha = T(0);
            return;
        }
        const T fs = std::max(T(Td::spu()), T(1));
        mAlpha = T(1) - std::exp(T(-1) / (mSmoothTime * fs));
    }

private:
    // 4x ZDF SVF stages
    TPTSVF<T,Td> lp1, lp2;
    TPTSVF<T,Td> hp1, hp2;

    // smoothing
    T mFcTarget = T(1000);
    T mFcZ      = T(1000);
    T mSmoothTime = T(0.01);
    T mAlpha      = T(0);
};

} // namespace gam
