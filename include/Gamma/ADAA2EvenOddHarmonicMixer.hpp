// ADAA2EvenOddHarmonicMixer.hpp
#pragma once
#include <cmath>
#include <limits>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {
namespace detail {

// ------------------------------
// Numeric helpers
// ------------------------------
template<class T>
inline T scaledEps(T a, T b){
    const T C = T(16);
    return C * std::numeric_limits<T>::epsilon() * (T(1) + scl::abs(a) + scl::abs(b));
}

template<class T>
inline T clamp01(T v){
    return (v < T(0)) ? T(0) : (v > T(1)) ? T(1) : v;
}

// ------------------------------
// Unit-slope atan saturator family
// f(x;k) = atan(kx)/k  (unity slope at 0)
// F(x;k) = ∫ f dx
// G(x;k) = ∫ F dx
// Closed-form, primitive-friendly (atan + log1p)
// ------------------------------
template<class T>
struct AtanSatUnitSlope
{
    static inline T f(T x, T k){
        const T ak = scl::abs(k);
        if(ak <= T(1e-8)) return x;                 // linear limit
        return std::atan(k * x) / k;
    }

    static inline T F(T x, T k){
        const T ak = scl::abs(k);
        if(ak <= T(1e-8)) return T(0.5) * x * x;    // ∫x dx

        const T kx = k * x;
        const T invk  = T(1) / k;
        const T invk2 = invk * invk;

        // F = x*atan(kx)/k - 0.5*log(1+(kx)^2)/k^2
        return x * std::atan(kx) * invk
             - T(0.5) * std::log1p(kx * kx) * invk2;
    }

    static inline T G(T x, T k){
        const T ak = scl::abs(k);
        if(ak <= T(1e-8)) return (T(1)/T(6)) * x * x * x; // ∫(0.5 x^2) dx = x^3/6

        const T kx = k * x;
        const T a  = std::atan(kx);
        const T l  = std::log1p(kx * kx);

        const T invk  = T(1) / k;
        const T invk2 = invk * invk;
        const T invk3 = invk2 * invk;

        // G(x) =
        // (x^2/(2k))atan(kx) - (x/(2k^2))ln(1+k^2x^2) + x/k^2 - (1/k^3)atan(kx)
        return (T(0.5) * x * x * invk) * a
             - (T(0.5) * x * invk2) * l
             + (x * invk2)
             - (invk3) * a;
    }
};

// ------------------------------
// ADAA1 + ADAA2 helpers (robust)
// Uses midpoint fallback when dx is tiny.
// ------------------------------
template<class T, class Sat>
inline T adaa1(T x0, T x1, T k){
    if(scl::abs(k) <= T(1e-8)){
        // exact linear: midpoint equals f for linear
        return T(0.5) * (x0 + x1);
    }

    const T dx  = x0 - x1;
    const T eps = scaledEps(x0, x1);

    if(scl::abs(dx) <= eps){
        return Sat::f(T(0.5) * (x0 + x1), k);
    }
    return (Sat::F(x0, k) - Sat::F(x1, k)) / dx;
}

template<class T, class Sat>
inline T adaa2(T x0, T x1, T x2, T k){
    if(scl::abs(k) <= T(1e-8)){
        // linear: 2nd order consistent fallback
        return T(0.5) * (x0 + x1);
    }

    const T dx01 = x0 - x1;
    const T dx12 = x1 - x2;
    const T dx02 = x0 - x2;

    const T e01 = scaledEps(x0, x1);
    const T e12 = scaledEps(x1, x2);
    const T e02 = scaledEps(x0, x2);

    // If the two-step collapses, fall back to ADAA1 on the most recent segment
    if(scl::abs(dx02) <= e02){
        return adaa1<T, Sat>(x0, x1, k);
    }

    // Secant quotients of G; when dx collapses, use dG/dx = F at midpoint
    T q01, q12;

    if(scl::abs(dx01) <= e01){
        q01 = Sat::F(T(0.5) * (x0 + x1), k);
    } else {
        q01 = (Sat::G(x0, k) - Sat::G(x1, k)) / dx01;
    }

    if(scl::abs(dx12) <= e12){
        q12 = Sat::F(T(0.5) * (x1 + x2), k);
    } else {
        q12 = (Sat::G(x1, k) - Sat::G(x2, k)) / dx12;
    }

    // Main ADAA2 formula
    return (T(2) / dx02) * (q01 - q12);
}

} // namespace detail

// ============================================================================
// ADAA2EvenOddHarmonicMixer
// - Unit-slope static nonlinearity family: atan(kx)/k
// - Odd harmonics = (y0 - xDriven)
// - Even harmonics = bias-delta: (yb - y0)
// - “Normalize only added harmonics”: optional soft normalization on the harmonic add term
// - ADAA2 on the nonlinearity (closed form, primitive-friendly)
// - Parameter-hold contract: hold drive/k/bias/odd/even/intensity per interval.
//   Optional: also hold post-mix params (mix/bodyMix) to avoid aliasing from those modulations.
// ============================================================================

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ADAA2EvenOddHarmonicMixer : public Td {
public:
    ADAA2EvenOddHarmonicMixer(){
        reset();
    }

    // ---------------------------
    // Parameters (live, can be modulated)
    // ---------------------------

    // Curvature k (>=0 is typical; sign works but usually unnecessary)
    void curvature(T k){ mK = k; }

    // Bias used to generate even harmonics via bias-delta
    void bias(T b){ mBias = b; }

    // Harmonic gains (0..1 typical, but not enforced)
    void odd(T a){ mOdd = a; }
    void even(T a){ mEven = a; }

    // Intensity applied to harmonic add only
    void intensity(T a){ mIntensity = a; }

    // Drive law: drive = 1 + driveScale * envHold
    void driveScale(T s){ mDriveScale = scl::max(s, T(0)); }

    // Envelope input (0..1 recommended). The held envelope is used for the current interval,
    // then the *next* envelope value is latched after output to maintain the ADAA contract.
    void env01(T e){ mEnvNext = detail::clamp01(e); }

    // Parallel mix (0..1)
    void mix(T m){ mMix = detail::clamp01(m); }

    // At mix=1: y = (1-bodyMix)*wet + bodyMix*dry
    void bodyMix(T b){ mBodyMix = detail::clamp01(b); }

    // If true, also hold mix/bodyMix per interval (reduces aliasing from post-ADAA modulation)
    void holdPostMixParams(bool b){ mHoldPost = b; }

    // Optional: compensate drive so increasing drive mostly increases saturation, not level
    // wetComp = wet / max(drive, eps)
    void compensateDrive(bool b){ mCompensateDrive = b; }

    // Optional: normalize harmonic-add only (keeps body stable as intensity increases)
    // addNorm = add / (1 + normStrength*|add|)
    void harmonicNormalize(bool b){ mHarmNormalize = b; }
    void harmonicNormStrength(T s){ mNormStrength = scl::max(s, T(0)); }

    // ---------------------------
    // Lifecycle
    // ---------------------------
    void reset(){
        mX1 = mX2 = T(0);
        mHave2 = false;

        mEnvHold = mEnvNext = T(0);

        // initialize holds
        mDriveHold = T(1);
        mKHold     = mK;
        mBiasHold  = mBias;
        mOddHold   = mOdd;
        mEvenHold  = mEven;
        mIntHold   = mIntensity;

        mMixHold   = mMix;
        mBodyHold  = mBodyMix;
    }

    void onDomainChange(double){
        // No coefficients derived from sample rate in this block.
        // (Kept for Gamma-style consistency / future extensions.)
    }

    // ---------------------------
    // Processing
    // ---------------------------
    inline T operator()(T x){
        // Use held parameters for THIS interval (ADAA contract)
        const T drive = mDriveHold;
        const T k     = mKHold;
        const T b     = mBiasHold;

        const T oddA  = mOddHold;
        const T evenA = mEvenHold;
        const T inten = mIntHold;

        const T mixV  = mHoldPost ? mMixHold  : mMix;
        const T bodyV = mHoldPost ? mBodyHold : mBodyMix;

        // Build driven inputs for ADAA evaluation
        const T x0 = x * drive;
        const T x1 = mX1 * drive;
        const T x2 = mX2 * drive;

        // Base ADAA output (odd-symmetric shaper)
        T y0;
        if(mHave2){
            y0 = detail::adaa2<T, detail::AtanSatUnitSlope<T>>(x0, x1, x2, k);
        } else {
            // startup: ADAA1 fallback until history exists
            y0 = detail::adaa1<T, detail::AtanSatUnitSlope<T>>(x0, x1, k);
        }

        // Bias-delta for even harmonics
        // (Use the same nonlinearity family + ADAA order)
        const T xb0 = x0 + b;
        const T xb1 = x1 + b;
        const T xb2 = x2 + b;

        T yb;
        if(mHave2){
            yb = detail::adaa2<T, detail::AtanSatUnitSlope<T>>(xb0, xb1, xb2, k);
        } else {
            yb = detail::adaa1<T, detail::AtanSatUnitSlope<T>>(xb0, xb1, k);
        }

        // Harmonic decomposition (orthogonal by construction)
        const T oddH  = (y0 - x0);
        const T evenH = (yb - y0);

        // Add harmonics only (body preserved)
        T add = inten * (oddA * oddH + evenA * evenH);

        // Optional: normalize only the added harmonics (does not touch the body)
        if(mHarmNormalize){
            // soft normalization that preserves sign and is cheap
            add = add / (T(1) + mNormStrength * scl::abs(add));
        }

        // Wet in driven domain
        T wet = x0 + add;

        // Optional drive compensation: keep wet near unity for small-signal
        if(mCompensateDrive){
            const T eps = T(1e-12);
            wet = wet / scl::max(drive, eps);
        }

        // Dry is the original signal (undriven)
        const T dry = x;

        // Parallel mix with body preservation contract
        // mix=0 -> dry
        // mix=1 -> (1-body)*wet + body*dry
        const T y = (T(1) - mixV) * dry
                  + mixV * ( (T(1) - bodyV) * wet + bodyV * dry );

        // ---- advance history (raw x) ----
        mX2 = mX1;
        mX1 = x;
        mHave2 = true;

        // ---- latch holds for NEXT interval ----
        // hold all params that affect f() and harmonic gains for modulation cleanliness
        mKHold    = mK;
        mBiasHold = mBias;
        mOddHold  = mOdd;
        mEvenHold = mEven;
        mIntHold  = mIntensity;

        // envelope hold contract: use previous envHold for this sample, then latch next
        mEnvHold  = mEnvNext;
        mDriveHold = T(1) + (mDriveScale * mEnvHold);

        // post-mix holds if requested
        if(mHoldPost){
            mMixHold  = mMix;
            mBodyHold = mBodyMix;
        }

        return y;
    }

private:
    // live params
    T mK         = T(1);
    T mBias      = T(0);

    T mOdd       = T(1);
    T mEven      = T(0);
    T mIntensity = T(1);

    T mDriveScale = T(0);

    T mMix     = T(1);
    T mBodyMix = T(0);

    bool mHoldPost = false;
    bool mCompensateDrive = false;

    bool mHarmNormalize = true;
    T    mNormStrength  = T(0.5);

    // envelope (held per interval)
    T mEnvNext = T(0);
    T mEnvHold = T(0);

    // held params (per interval)
    T mDriveHold = T(1);
    T mKHold     = T(1);
    T mBiasHold  = T(0);
    T mOddHold   = T(1);
    T mEvenHold  = T(0);
    T mIntHold   = T(1);

    T mMixHold   = T(1);
    T mBodyHold  = T(0);

    // history (raw input samples)
    T mX1 = T(0);
    T mX2 = T(0);
    bool mHave2 = false;
};

} // namespace gam
