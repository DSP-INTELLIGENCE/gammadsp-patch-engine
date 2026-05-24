#pragma once
#include <cmath>
#include <algorithm>
#include <limits>
#include <type_traits>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

// ============================================================
// Soft diode curve with exact 1st + 2nd primitives
// ============================================================
template<typename T>
struct SoftDiodeKnee {
    static_assert(std::is_floating_point_v<T>, "SoftDiodeKnee<T>: T must be float/double");

    struct Params {
        T ap; // positive knee threshold (>0)
        T an; // negative knee threshold (>0)
    };

    static inline T transfer(T x, const Params& p){
        const T ap = p.ap;
        const T an = p.an;

        if(x > ap){
            const T d = x - ap;
            return ap + d / (T(1) + d*d);
        }
        if(x < -an){
            const T d = x + an;
            return -an + d / (T(1) + d*d);
        }
        return x;
    }

    // 1st primitive F(x) with continuity constants:
    // Mid:    F = x^2/2
    // Upper:  F = ap*x + 0.5*ln(1+(x-ap)^2) - ap^2/2
    // Lower:  F = -an*x + 0.5*ln(1+(x+an)^2) - an^2/2
    static inline T primitive1(T x, const Params& p){
        const T ap = p.ap;
        const T an = p.an;

        if(x > ap){
            const T d = x - ap;
            return ap*x + T(0.5)*std::log(T(1) + d*d) - T(0.5)*ap*ap;
        }
        if(x < -an){
            const T d = x + an;
            return -an*x + T(0.5)*std::log(T(1) + d*d) - T(0.5)*an*an;
        }
        return T(0.5)*x*x;
    }

    // 2nd primitive G(x) = ∫ F(x) dx, continuous at knees:
    // Mid:    G = x^3/6
    //
    // Helper: I(d) = ∫ 0.5*ln(1+d^2) dd
    //             = 0.5*d*ln(1+d^2) - d + atan(d)
    //
    // Upper:
    //   G = 0.5*ap*x^2 + I(x-ap) - 0.5*ap^2*x + ap^3/6
    //
    // Lower:
    //   G = -0.5*an*x^2 + I(x+an) - 0.5*an^2*x - an^3/6
    static inline T primitive2(T x, const Params& p){
        const T ap = p.ap;
        const T an = p.an;

        auto I = [](T d){
            return T(0.5)*d*std::log(T(1) + d*d) - d + std::atan(d);
        };

        if(x > ap){
            const T d = x - ap;
            return T(0.5)*ap*x*x + I(d) - T(0.5)*ap*ap*x + (ap*ap*ap)/T(6);
        }
        if(x < -an){
            const T d = x + an;
            return -T(0.5)*an*x*x + I(d) - T(0.5)*an*an*x - (an*an*an)/T(6);
        }
        return (x*x*x)/T(6);
    }
};


// ============================================================
// ADAA1 diode clipper (exact)
// ============================================================
template<
    class T = gam::real,
    template<typename> class Curve = SoftDiodeKnee,
    class Td = GAM_DEFAULT_DOMAIN
>
class DiodeClipperADAA1 : public Td {
    static_assert(std::is_floating_point_v<T>, "DiodeClipperADAA1<T>: T must be float/double");

public:
    using CurveT  = Curve<T>;
    using Params  = typename CurveT::Params;

    DiodeClipperADAA1(){ reset(); }

    void reset(){
        mHas1 = false;
        mX1   = T(0);
    }

    void onDomainChange(double){}

    // ---- instantaneous parameters (no smoothing) ----
    void drive(T v)     { mDrive = std::max(v, T(0)); }
    void threshold(T v) { mThresh = std::max(v, T(1e-9)); }

    // asym ∈ [-1,1]: makes + and - thresholds differ (even harmonics), exact primitives preserved
    void asymmetry(T v) { mAsym = scl::clip(v, T(-1), T(1)); }

    void trim(T v)      { mTrim = v; }

    // ---- process ----
    inline T operator()(T x){
        if(!std::isfinite(x)) return T(0);

        // Pre-scale (ADAA operates on this driven input)
        const T u = x * mDrive;

        Params p = params();

        const T y = adaa1(u, p);

        // Output scaling
        return y * mTrim;
    }

private:
    // thresholds from asym (keep >0)
    Params params() const {
        // Map asym so + and - thresholds remain positive and bounded
        // ap = thresh*(1 - 0.5*asym), an = thresh*(1 + 0.5*asym)
        const T s  = mAsym;
        const T ap = std::max(mThresh * (T(1) - T(0.5)*s), T(1e-9));
        const T an = std::max(mThresh * (T(1) + T(0.5)*s), T(1e-9));
        return Params{ ap, an };
    }

    static inline T scaledEps(T x0, T x1){
        const T C = T(16);
        return C * std::numeric_limits<T>::epsilon() * (T(1) + std::abs(x0) + std::abs(x1));
    }

    inline T adaa1(T x0, const Params& p){
        if(!mHas1){
            mHas1 = true;
            mX1   = x0;
            return CurveT::transfer(x0, p);
        }

        const T x1 = mX1;
        mX1 = x0;

        const T dx  = x0 - x1;
        const T eps = scaledEps(x0, x1);

        if(std::abs(dx) <= eps){
            // small-step limit -> evaluate at midpoint (exact for limit)
            return CurveT::transfer(T(0.5)*(x0 + x1), p);
        }

        const T F0 = CurveT::primitive1(x0, p);
        const T F1 = CurveT::primitive1(x1, p);
        const T y  = (F0 - F1) / dx;

        return std::isfinite(y) ? y : T(0);
    }

private:
    // state
    bool mHas1 = false;
    T    mX1   = T(0);

    // params
    T mDrive  = T(1);
    T mThresh = T(0.6);
    T mAsym   = T(0);
    T mTrim   = T(1);
};


// ============================================================
// ADAA2 diode clipper (exact)
// ============================================================
template<
    class T = gam::real,
    template<typename> class Curve = SoftDiodeKnee,
    class Td = GAM_DEFAULT_DOMAIN
>
class DiodeClipperADAA2 : public Td {
    static_assert(std::is_floating_point_v<T>, "DiodeClipperADAA2<T>: T must be float/double");

public:
    using CurveT  = Curve<T>;
    using Params  = typename CurveT::Params;

    DiodeClipperADAA2(){ reset(); }

    void reset(){
        mCount = 0;
        mX1 = mX2 = T(0);
    }

    void onDomainChange(double){}

    // ---- instantaneous parameters (no smoothing) ----
    void drive(T v)     { mDrive = std::max(v, T(0)); }
    void threshold(T v) { mThresh = std::max(v, T(1e-9)); }
    void asymmetry(T v) { mAsym = scl::clip(v, T(-1), T(1)); }
    void trim(T v)      { mTrim = v; }

    inline T operator()(T x){
        if(!std::isfinite(x)) return T(0);

        const T u = x * mDrive;
        Params p = params();

        // startup: fall back cleanly
        if(mCount == 0){
            mX1 = u; mCount = 1;
            return CurveT::transfer(u, p) * mTrim;
        }
        if(mCount == 1){
            mX2 = mX1;
            mX1 = u;
            mCount = 2;
            // ADAA1 on first two samples
            return adaa1(u, mX2, p) * mTrim;
        }

        // steady-state ADAA2
        const T y = adaa2(u, p);
        return y * mTrim;
    }

private:
    Params params() const {
        const T s  = mAsym;
        const T ap = std::max(mThresh * (T(1) - T(0.5)*s), T(1e-9));
        const T an = std::max(mThresh * (T(1) + T(0.5)*s), T(1e-9));
        return Params{ ap, an };
    }

    static inline T scaledEps3(T x0, T x1, T x2){
        const T C = T(32);
        return C * std::numeric_limits<T>::epsilon() * (T(1) + std::abs(x0) + std::abs(x1) + std::abs(x2));
    }

    // ADAA1 helper used during warmup / degeneracy
    static inline T adaa1(T x0, T x1, const Params& p){
        const T dx = x0 - x1;
        const T eps = T(16) * std::numeric_limits<T>::epsilon() * (T(1) + std::abs(x0) + std::abs(x1));
        if(std::abs(dx) <= eps){
            return CurveT::transfer(T(0.5)*(x0 + x1), p);
        }
        const T F0 = CurveT::primitive1(x0, p);
        const T F1 = CurveT::primitive1(x1, p);
        const T y  = (F0 - F1) / dx;
        return std::isfinite(y) ? y : T(0);
    }

    inline T adaa2(T x0, const Params& p){
        // x0 = current, x1 = prev, x2 = prev2
        const T x1 = mX1;
        const T x2 = mX2;

        // shift history
        mX2 = mX1;
        mX1 = x0;

        const T d01 = x0 - x1;
        const T d12 = x1 - x2;
        const T d02 = x0 - x2;

        const T eps = scaledEps3(x0, x1, x2);

        // Degenerate cases -> fall back to ADAA1 (still exact)
        if(std::abs(d01) <= eps || std::abs(d12) <= eps || std::abs(d02) <= eps){
            return adaa1(x0, x1, p);
        }

        // ADAA2: y = 2 * G[x0,x1,x2]
        // where G[x0,x1] = (G0 - G1)/(x0 - x1)
        // and   G[x0,x1,x2] = (G[x0,x1] - G[x1,x2])/(x0 - x2)
        const T G0  = CurveT::primitive2(x0, p);
        const T G1  = CurveT::primitive2(x1, p);
        const T G2  = CurveT::primitive2(x2, p);

        const T G01 = (G0 - G1) / d01;
        const T G12 = (G1 - G2) / d12;

        const T y   = T(2) * (G01 - G12) / d02;

        return std::isfinite(y) ? y : T(0);
    }

private:
    // history
    int mCount = 0;
    T   mX1 = T(0);
    T   mX2 = T(0);

    // params
    T mDrive  = T(1);
    T mThresh = T(0.6);
    T mAsym   = T(0);
    T mTrim   = T(1);
};

} // namespace gam
