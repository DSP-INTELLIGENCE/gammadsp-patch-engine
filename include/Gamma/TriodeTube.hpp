#pragma once
#include <cmath>
#include <algorithm>
#include <type_traits>

namespace gdsp {

template<typename T>
class TriodeTube {
    static_assert(std::is_floating_point_v<T>, "TriodeTube<T>: T must be float/double");

public:
    TriodeTube() = default;

    // ---------- lifecycle ----------
    void prepare(T sampleRate) {
        mFs = std::max(sampleRate, T(1));
        updateSmoothers();
        reset();
    }

    void reset() {
        mCath = T(0);
        mPlate = T(0);
        mLastIn = T(0);
        mLastOut = T(0);

        // ADAA history (internal-domain)
        mLastV = T(0);
        mLastSatEff = mSat;   // initialize reasonably
        mLastAsym = mAsym;
    }

    // ---------- parameters ----------
    void setDrive(T drive)      { mDrive = std::max(drive, T(0)); }
    void setBias(T bias)        { mBias  = bias; }
    void setAsymmetry(T asym)   { mAsym  = clamp(asym, T(-1), T(1)); }  // -1..1
    void setSaturation(T sat)   { mSat   = std::max(sat, T(0)); }       // curvature
    void setOutputTrim(T trim)  { mTrim  = trim; }
    void setCalibration(T ref)  { mRef   = std::max(ref, T(1e-6)); }    // nominal level

    // Tier 3 toggles
    void enableStateful(bool e) { mStateful = e; }

    // Time constants (seconds) for stateful mode
    void setMemoryTimes(T attackSec, T releaseSec) {
        mAtk = std::max(attackSec, T(1e-6));
        mRel = std::max(releaseSec, T(1e-6));
        updateSmoothers();
    }

    void setMemoryDepth(T depth) { mMemDepth = std::max(depth, T(0)); }

    // ---------- processing (plain) ----------
    T processSample(T x) {
        T v = prewarpInput_(x);
        T y = coreStage_(v);
        y = postwarpOutput_(y);
        mLastIn = x;
        mLastOut = y;
        return y;
    }

    // ---------- processing (ADAA1) ----------
    // Exact ADAA1 for the *memoryless transfer*.
    // Works best when stateful is off or gentle (time-varying params reduce “perfect” ADAA).
    T processSampleADAA1(T x) {
        T v = prewarpInput_(x);

        // Update state (Tier 3) and compute current effective sat for this sample
        updateState_(v);

        // ADAA1 over memoryless f(v; satEff, asym)
        const T yCore = adaa1_(v, mSatEff, mAsym);

        T y = postwarpOutput_(yCore);

        mLastIn = x;
        mLastOut = y;

        // update ADAA history
        mLastV      = v;
        mLastSatEff = mSatEff;
        mLastAsym   = mAsym;

        return y;
    }

    // Optional: expose last values
    T lastIn() const { return mLastIn; }
    T lastOut() const { return mLastOut; }

    // ---------- ADAA seam (f and F) ----------
    T transferADAA(T x) const {
        return transfer_(x, mSatEff, mAsym);
    }

    T transferADAA_primitive(T x) const {
        return transferPrimitive_(x, mSatEff, mAsym);
    }

private:
    // ---------- utilities ----------
    static T clamp(T v, T lo, T hi) { return std::min(hi, std::max(lo, v)); }

    static T soft01(T z) {
        z = std::max(z, T(0));
        return z / (T(1) + z);
    }

    // log(cosh(u)) in a numerically stable-ish way:
    // for large |u|, cosh(u) ~ 0.5*exp(|u|) so logcosh(u) ~ |u| - log(2)
    static T logcosh_(T u) {
        const T au = std::abs(u);
        if (au < T(20)) {
            return std::log(std::cosh(u));
        }
        // asymptotic
        return au - T(0.6931471805599453094); // log(2)
    }

    // ---------- primitive-friendly triode-ish transfer ----------
    //
    // u = k*x, k = 1 + sat  (stable mapping)
    // f(x) = tanh(u) + asym * tanh(u)^2
    //
    // Exact primitive:
    // F(x) = logcosh(u)/k + asym * ( x - tanh(u)/k )
    static T transfer_(T x, T sat, T asym) {
        const T k = T(1) + sat;
        const T u = k * x;
        const T t = std::tanh(u);
        return t + asym * (t * t);
    }

    static T transferPrimitive_(T x, T sat, T asym) {
        const T k = T(1) + sat;
        const T u = k * x;
        const T t = std::tanh(u);

        const T Fodd  = logcosh_(u) / k;
        const T Feven = x - (t / k); // ∫tanh^2(kx)dx
        return Fodd + asym * Feven;
    }

    // ADAA1 evaluator for current sample, with small-delta safety.
    // Uses the *current* (sat, asym). If these vary sample-to-sample (stateful),
    // the result is still stable, but not “theoretical perfect ADAA”.
    static T adaa1Eval_(T x, T x1, T sat, T asym) {
        const T dx = x - x1;
        // threshold scaled to typical float epsilon; tune if needed
        const T eps = T(1e-8);
        if (std::abs(dx) < eps) {
            // midpoint fallback reduces bias vs using f(x)
            const T xm = (x + x1) * T(0.5);
            return transfer_(xm, sat, asym);
        }
        const T F  = transferPrimitive_(x,  sat, asym);
        const T F1 = transferPrimitive_(x1, sat, asym);
        return (F - F1) / dx;
    }

    // ---------- signal path helpers ----------
    T prewarpInput_(T x) const {
        // Normalize to internal operating level
        T v = x / mRef;

        // input gain staging
        v *= mDrive;

        // bias (grid offset)
        v += mBias;

        return v;
    }

    T postwarpOutput_(T yCore) const {
        // output trim and de-normalize
        T y = yCore * mTrim;
        y *= mRef;
        return y;
    }

    // Tier 3 memory update; also computes mSatEff
    void updateState_(T& v) {
        if (mStateful) {
            const T e = v * v;
            const T a = (e > mCath) ? mAlphaAtk : mAlphaRel;
            mCath += a * (e - mCath);

            const T mem = mMemDepth * soft01(mCath);
            v += mem * T(0.25);

            mPlate += a * (mem - mPlate);
            mSatEff = mSat * (T(1) + mPlate * T(0.5));
        } else {
            mSatEff = mSat;
        }
    }

    // Plain (non-ADAA) core stage
    T coreStage_(T v) {
        updateState_(v);
        return transfer_(v, mSatEff, mAsym);
    }

    // ADAA1 core stage with history
    T adaa1_(T v, T satEff, T asym) {
        // If you want to be stricter, you can choose to use last sample’s sat/asym
        // or average them; current choice is “use current params” (stable, simple).
        return adaa1Eval_(v, mLastV, satEff, asym);
    }

    void updateSmoothers() {
        auto alpha = [&](T tau) {
            return T(1) - std::exp(T(-1) / (tau * mFs));
        };
        mAlphaAtk = alpha(mAtk);
        mAlphaRel = alpha(mRel);
    }

private:
    // sample rate
    T mFs = T(48000);

    // params
    T mDrive  = T(1);
    T mBias   = T(0);
    T mAsym   = T(0);
    T mSat    = T(1);
    T mSatEff = T(1);
    T mTrim   = T(1);
    T mRef    = T(1);

    // tier 3 state
    bool mStateful = false;
    T mAtk = T(0.002);
    T mRel = T(0.08);
    T mAlphaAtk = T(0.0);
    T mAlphaRel = T(0.0);
    T mMemDepth = T(1.0);

    T mCath  = T(0);
    T mPlate = T(0);

    // ADAA history (internal domain)
    T mLastV      = T(0);
    T mLastSatEff = T(1);
    T mLastAsym   = T(0);

    // debug
    T mLastIn  = T(0);
    T mLastOut = T(0);
};

} // namespace gdsp
