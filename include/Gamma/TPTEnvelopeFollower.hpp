#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

#include "TPT1Pole.hpp"

namespace gam {


// ------------------------------------------------------------
// EnvelopeFollowerTPT
// - TPT/ZDF-safe detector smoothing
// - Attack/Release in ms (sample-rate invariant)
// - Optional detector high-pass (sidechain HPF)
// ------------------------------------------------------------
template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class TPTEnvelopeFollower : public Td {
public:
    enum Mode { PEAK, RMS };

    TPTEnvelopeFollower(T attackMs = T(10), T releaseMs = T(150))
    {
        setAttackMs(attackMs);
        setReleaseMs(releaseMs);
        setMode(PEAK);
        setSidechainHPHz(T(0));   // off by default
        reset();
        recalc();
    }

    void reset()
    {
        mEnv = T(0);
        mSC.reset(T(0));
        mLP.reset(T(0));
    }

    // --- Settings ---
    void setMode(Mode m) { mMode = m; }

    void setAttackMs(T ms)
    {
        mAtkMs = std::max(ms, T(0.01));
        recalc();
    }

    void setReleaseMs(T ms)
    {
        mRelMs = std::max(ms, T(0.01));
        recalc();
    }

    // Optional detector-side HPF (prevents LF pumping)
    void setSidechainHPHz(T hz)
    {
        mSideHP = std::max(hz, T(0));
        recalc();
    }

    // Returns linear envelope (0..~1 for normalized signals)
    inline T process(T x)
    {
        // --- detector path (sidechain) ---
        if (mSideHP > T(0))
            x = mSC.hp(x);

        // --- rectify / detect ---
        T d;
        if (mMode == RMS) {
            // RMS: smooth power then sqrt (more “loudness-ish”)
            const T p = x * x;
            d = p;
        } else {
            // PEAK: full-wave rectified amplitude (snappier)
            d = std::fabs(x);
        }

        // --- attack/release selection (dynamic coefficient) ---
        // We implement A/R by swapping the 1-pole coefficient based on direction.
        const T a = (d > mEnv) ? mAtkA : mRelA;

        // TPT one-pole LP with dynamic 'a':
        // v = (d - s) * a; y = v + s; s = y + v
        // We'll use mLP state but override coefficient per-sample.
        mLP.a = a;
        T y = mLP.lp(d);

        if (mMode == RMS)
            y = std::sqrt(std::max(y, T(0)));

        mEnv = y;
        return mEnv;
    }

    inline T operator()(T x) { return process(x); }

    // Helpers
    T value() const { return mEnv; }

    // Convert to dBFS (with floor)
    T valueDb(T floorDb = T(-120)) const
    {
        const T eps = std::pow(T(10), floorDb / T(20));
        const T v = std::max(mEnv, eps);
        return T(20) * std::log10(v);
    }

    void onDomainChange(double) { recalc(); }

private:
    // Convert ms time constant to one-pole coefficient using TPT mapping.
    // For a simple exponential smoother, alpha = 1 - exp(-1/(tau*fs)).
    // We then map that into a TPT coefficient space via a cutoff equivalence:
    // Use fc = 1/(2*pi*tau) and compute a = g/(1+g).
    void recalc()
    {
        const T fs = T(Td::spu());

        // Sidechain HPF (if enabled)
        if (mSideHP > T(0)) mSC.freq(mSideHP);
        else                mSC.freq(T(1e-4)); // irrelevant when bypassed

        // Attack/release -> tau (seconds)
        const T atk = mAtkMs * T(0.001);
        const T rel = mRelMs * T(0.001);

        // Equivalent analog cutoff for given tau: fc = 1/(2*pi*tau)
        const T fcAtk = T(1) / (T(2) * T(M_PI) * std::max(atk, T(1e-6)));
        const T fcRel = T(1) / (T(2) * T(M_PI) * std::max(rel, T(1e-6)));

        // Clamp within usable range
        const T fcMax = T(0.495) * fs;
        mAtkA = cutoffToA(scl::clip(fcAtk, T(1e-4), fcMax), fs);
        mRelA = cutoffToA(scl::clip(fcRel, T(1e-4), fcMax), fs);

        // Prime LP coefficient (will be overridden dynamically anyway)
        mLP.a = mRelA;
    }

    static inline T cutoffToA(T fc, T fs)
    {
        const T g = std::tan(T(M_PI) * fc / fs);
        return g / (T(1) + g);
    }

private:
    Mode mMode = PEAK;

    // user params
    T mAtkMs  = T(10);
    T mRelMs  = T(150);
    T mSideHP = T(0);

    // coefficients
    T mAtkA = T(0.01);
    T mRelA = T(0.001);

    // state
    T mEnv = T(0);

    // internal filters
    TPT1Pole<T, Td> mSC; // sidechain HP
    TPT1Pole<T, Td> mLP; // envelope LP state
};

} // namespace gam
