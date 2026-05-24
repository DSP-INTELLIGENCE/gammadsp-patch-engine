#pragma once
#include <cmath>
#include <algorithm>
#include <limits>
#include <type_traits>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

template<
    typename T = gam::real,
    template<typename> class Curve = TriodeCurveAtanMix,
    class Td = GAM_DEFAULT_DOMAIN
>
class TriodeTubeADAA2 : public Td {
    static_assert(std::is_floating_point_v<T>, "TriodeTubeADAA2<T>: T must be float/double");

public:
    using CurveT = Curve<T>;
    using Params = typename CurveT::Params;

    TriodeTubeADAA2(){
        reset();
        onDomainChange(1.0);
    }

    // Gamma lifecycle
    void reset(){
        mX1 = mX2 = T(0);
        mHas1 = mHas2 = false;
    }

    void onDomainChange(double){
        // no SR dependence required for ADAA itself
    }

    // Parameters (same semantics as your ADAA1 v2)
    void drive(T v)       { mDrive = std::max(v, T(0)); }
    void saturation(T v)  { mSat01 = scl::clip(v, T(0), T(1)); }
    void bias(T v)        { mBias  = v; }
    void asymmetry(T v)   { mAsym  = scl::clip(v, T(-1), T(1)); }
    void trim(T v)        { mTrim  = v; }
    void calibration(T r) { mRef   = std::max(r, kRefMin); }
    void maxCurvature(T kMax){ mKMax = std::max(kMax, kMin()); }

    // Main process
    inline T operator()(T x){
        if(!std::isfinite(x)) return T(0);

        // normalize + pre-gain
        const T x0 = (x / mRef) * mDrive;

        Params p;
        p.k    = curvatureFromSat(mSat01);
        p.bias = mBias;
        p.asym = mAsym;

        // ADAA2 core (falls back to ADAA1 or transfer when needed)
        const T y = adaa2(x0, p);

        // de-normalize
        return (y * mTrim) * mRef;
    }

private:
    static constexpr T kRefMin = T(1e-6);
    static constexpr T kMinV   = T(1e-6);
    static constexpr T kMin(){ return kMinV; }

    // sat ∈ [0,1] → curvature k (same perceptual exp2 mapping)
    T curvatureFromSat(T s) const {
        const T kLo = kMin();
        const T kHi = std::max(mKMax, kLo);

        if(s <= T(0)) return kLo;
        if(s >= T(1)) return kHi;

        const T a   = T(6);
        const T num = std::exp2(a * s) - T(1);
        const T den = std::exp2(a)     - T(1);
        const T t   = num / den;
        return kLo + (kHi - kLo) * t;
    }

    // magnitude-scaled epsilon (robust across float/double & scaling)
    static T scaledEps(T a, T b){
        const T C = T(16);
        return C * std::numeric_limits<T>::epsilon()
             * (T(1) + std::abs(a) + std::abs(b));
    }

    // exact ADAA1 helper using F (first primitive)
    static T adaa1_exact(T x0, T x1, const Params& p){
        const T dx  = x0 - x1;
        const T eps = scaledEps(x0, x1);

        if(std::abs(dx) <= eps){
            // stable small-step fallback
            return CurveT::transfer(T(0.5) * (x0 + x1), p);
        }
        const T F0 = CurveT::primitive(x0, p);
        const T F1 = CurveT::primitive(x1, p);
        const T y  = (F0 - F1) / dx;
        return std::isfinite(y) ? y : T(0);
    }

    // ADAA2 = 2 * second divided difference of G (second primitive)
    T adaa2(T x0, const Params& p){
        // shift history: x2 <- x1 <- x0
        if(!mHas1){
            mX1 = x0; mHas1 = true;
            return CurveT::transfer(x0, p);
        }
        if(!mHas2){
            const T y = adaa1_exact(x0, mX1, p);
            mX2 = mX1; mX1 = x0; mHas2 = true;
            return y;
        }

        const T x1 = mX1;
        const T x2 = mX2;

        // update history for next time
        mX2 = mX1;
        mX1 = x0;

        const T dx01 = x0 - x1;
        const T dx12 = x1 - x2;
        const T dx02 = x0 - x2;

        const T eps01 = scaledEps(x0, x1);
        const T eps12 = scaledEps(x1, x2);
        const T eps02 = scaledEps(x0, x2);

        // If any denominator is tiny, fall back to ADAA1 (still exact ADAA1)
        if(std::abs(dx01) <= eps01 || std::abs(dx12) <= eps12 || std::abs(dx02) <= eps02){
            return adaa1_exact(x0, x1, p);
        }

        // First divided differences of G
        const T G0 = CurveT::primitive2(x0, p);
        const T G1 = CurveT::primitive2(x1, p);
        const T G2 = CurveT::primitive2(x2, p);

        const T dG01 = (G0 - G1) / dx01;
        const T dG12 = (G1 - G2) / dx12;

        // Second divided difference of G
        const T ddG = (dG01 - dG12) / dx02;

        // For smooth G, ddG ≈ f/2, so multiply by 2
        const T y = T(2) * ddG;

        return std::isfinite(y) ? y : T(0);
    }

private:
    // history (x[n-1], x[n-2])
    T    mX1 = T(0), mX2 = T(0);
    bool mHas1 = false, mHas2 = false;

    // params
    T mDrive = T(1);
    T mSat01 = T(0.5);
    T mBias  = T(0);
    T mAsym  = T(0);
    T mTrim  = T(1);
    T mRef   = T(1);
    T mKMax  = T(16);
};

} // namespace gam
