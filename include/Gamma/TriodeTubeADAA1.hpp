#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<typename T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class TriodeTubeADAA1 : public Td {
    static_assert(std::is_floating_point_v<T>,
        "TriodeTubeADAA1<T>: T must be float/double");

public:
    TriodeTubeADAA1(){
        reset();
    }

    // ------------------------------------------------------------
    // Gamma lifecycle
    // ------------------------------------------------------------
    void reset(){
        mPrevX = T(0);
        mHasPrev = false;
        mCath.reset();
        mBiasEffPrev = mBias;
    }

    void onDomainChange(double){
        mFs = std::max(T(Td::spu()), T(1));
        mCath.recalc(mFs);
    }

    // ------------------------------------------------------------
    // Parameters
    // ------------------------------------------------------------
    void setDrive(T v)      { mDrive = std::max(v, T(0)); }
    void setSaturation(T v) { mSat   = std::max(v, T(0)); }
    void setBias(T v)       { mBias  = v; }
    void setAsymmetry(T v)  { mAsym  = clamp(v, T(-1), T(1)); }
    void setOutputTrim(T v) { mTrim  = v; }
    void setCalibration(T v){ mRef   = std::max(v, T(1e-6)); }

    // ---- Cathode memory ----
    void setCathodeMemoryEnabled(bool v){ mCath.enabled = v; }
    void setCathodeMemoryAmount(T v){ mCath.amount = std::max(v, T(0)); }
    void setCathodeMemoryThreshold(T v){ mCath.thresh = std::max(v, T(0)); }

    void setCathodeMemoryChargeMs(T ms){
        mCath.tauChargeMs = std::max(ms, T(0.01));
        mCath.recalc(mFs);
    }

    void setCathodeMemoryReleaseMs(T ms){
        mCath.tauRelMs = std::max(ms, T(0.01));
        mCath.recalc(mFs);
    }

    void setCathodeMemoryLeakPerSec(T v){
        mCath.leakPerSec = std::max(v, T(0));
        mCath.recalc(mFs);
    }

    void setCathodeMemoryMode(int mode){
        mCath.mode = (mode == 1) ? 1 : 0;
    }

    // ------------------------------------------------------------
    // Audio processing (Gamma style)
    // ------------------------------------------------------------
    inline T operator()(T x){
        // normalize & pre-gain
        const T xn = (x / mRef) * mDrive;

        // hold effective bias for ADAA step
        const T bHold = mBiasEffPrev;

        // ADAA1 nonlinear
        T y = adaa1_(xn, bHold);

        // update cathode memory *after* ADAA
        mCath.update(xn);

        // compute bias for next sample
        mBiasEffPrev = mBias + (mCath.enabled ? mCath.biasShift() : T(0));

        // post trim & denormalize
        y *= mTrim;
        y *= mRef;
        return y;
    }

    // ------------------------------------------------------------
    // Debug / taps
    // ------------------------------------------------------------
    T transfer(T x)  const { return transfer_(x, mBias); }
    T primitive(T x) const { return primitive_(x, mBias); }
    T cathodeState() const { return mCath.vk; }
    T effectiveBiasHeld() const { return mBiasEffPrev; }

private:
    // ============================================================
    // Cathode memory (program-dependent bias sag)
    // ============================================================
    struct CathodeMemory {
        bool enabled = true;

        T vk = T(0);

        T amount      = T(0.25);
        T thresh      = T(0.05);
        T tauChargeMs = T(5);
        T tauRelMs    = T(120);
        T leakPerSec  = T(0);
        int mode      = 1;

        T aCh = T(0), aRel = T(0), leakA = T(1);

        void reset(){ vk = T(0); }

        void recalc(T fs){
            if(fs <= T(0)) fs = T(1);

            auto alpha = [&](T ms){
                const T tau = std::max(ms, T(0.01)) * T(0.001);
                return T(1) - std::exp(T(-1) / (fs * tau));
            };

            aCh   = alpha(tauChargeMs);
            aRel  = alpha(tauRelMs);
            leakA = (leakPerSec > T(0))
                ? std::exp(-leakPerSec / fs)
                : T(1);
        }

        T detector(T x) const {
            if(mode == 1)
                return std::max(T(0), x - thresh);
            return std::max(T(0), std::abs(x) - thresh);
        }

        void update(T x){
            if(!enabled) return;
            const T d = detector(x);
            const T a = (d > vk) ? aCh : aRel;
            vk += a * (d - vk);
            vk *= leakA;
        }

        T biasShift() const { return -amount * vk; }
    };

    // ============================================================
    // ADAA + waveshaping
    // ============================================================
    static T clamp(T v, T lo, T hi){
        return std::min(hi, std::max(lo, v));
    }

    T curvature_() const {
        const T kMin = T(0.05);
        const T kMax = T(10.0);
        const T t = mSat / (T(1) + mSat);
        return kMin + (kMax - kMin) * t;
    }

    static T logcosh_(T z){
        const T az = std::abs(z);
        return az + std::log1p(std::exp(T(-2) * az)) - std::log(T(2));
    }

    T transfer_(T x, T b) const {
        const T k = curvature_();
        const T wp = T(0.5) * (T(1) + mAsym);
        const T wm = T(0.5) * (T(1) - mAsym);
        return wp * std::tanh(k * (x + b))
             + wm * std::tanh(k * (x - b));
    }

    T primitive_(T x, T b) const {
        const T k = curvature_();
        if(k <= T(1e-12))
            return T(0.5) * x * x;

        const T wp = T(0.5) * (T(1) + mAsym);
        const T wm = T(0.5) * (T(1) - mAsym);

        return (wp * logcosh_(k * (x + b))
              + wm * logcosh_(k * (x - b))) / k;
    }

    T adaa1_(T x, T b){
        if(!mHasPrev){
            mPrevX = x;
            mHasPrev = true;
            return transfer_(x, b);
        }

        const T x1 = mPrevX;
        mPrevX = x;

        const T dx = x - x1;
        const T mag = T(1) + std::abs(x) + std::abs(x1);
        const T eps = T(16) * std::numeric_limits<T>::epsilon() * mag;

        if(std::abs(dx) < eps){
            return transfer_(T(0.5) * (x + x1), b);
        }

        return (primitive_(x, b) - primitive_(x1, b)) / dx;
    }

private:
    // state
    T    mPrevX = T(0);
    bool mHasPrev = false;
    T    mBiasEffPrev = T(0);

    CathodeMemory mCath;

    // params
    T mFs    = T(48000);
    T mDrive = T(1);
    T mSat   = T(1);
    T mBias  = T(0);
    T mAsym  = T(0);
    T mTrim  = T(1);
    T mRef   = T(1);
};

} // namespace gam
