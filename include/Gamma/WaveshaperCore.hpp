// WaveshaperCore.hpp
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <type_traits>

namespace ws {

// -----------------------------
// Utilities
// -----------------------------
template<class T>
static inline T clamp01(T v){ return std::clamp(v, T(0), T(1)); }

template<class T>
static inline T clampSym(T v){ return std::clamp(v, T(-1), T(1)); }

template<class T>
static inline T safeAbs(T v){ return std::abs(v); }

template<class T>
static inline T smoothstep01(T t){
    t = clamp01(t);
    return t * t * (T(3) - T(2) * t);
}

// Fast-ish exp clamp helper (for diode-ish curve)
template<class T>
static inline T safeExp(T x){
    // keep exp in a sane range
    x = std::clamp(x, T(-50), T(50));
    return std::exp(x);
}

// -----------------------------
// Params passed to curve f(x,p)
// -----------------------------
template<class T>
struct Params {
    // canonical core controls (normalized “shaper domain”)
    T drive     = T(1);      // pre-gain multiplier
    T bias      = T(0);      // DC shift in shaper domain
    T asym      = T(0);      // -1..+1 (negative favors -, positive favors +)
    T threshold = T(1);      // knee/threshold subsystem uses this
    T knee      = T(0);      // 0..1 normalized knee width
    T harmonics = T(0);      // 0..1 optional harmonic enhancer

    // optional curve-specific knobs (use if needed)
    T hardness  = T(0);      // e.g. diode steepness / tube “sag”
};

// -----------------------------
// Knee / Threshold Mapper
// Maps x -> x' such that:
//  - inside |x| <= threshold : mostly linear
//  - outside                 : progressively “hits” the curve
// This keeps curve code clean.
// -----------------------------
template<class T>
class KneeMapper {
public:
    void setThreshold(T t){ mThr = std::max(T(0), t); }
    void setKnee(T k){ mKnee = clamp01(k); }

    // returns mapped x' and also an optional blend (0..1) indicating “in knee”
    struct Out { T x; T blend; };

    inline Out operator()(T x) const {
        const T thr = std::max(mThr, T(1e-12));
        const T ax  = safeAbs(x);

        if(mKnee <= T(0)){
            // hard knee: purely linear up to threshold, then scale beyond
            if(ax <= thr) return {x, T(0)};
            // outside: normalize so curve sees threshold at ~1
            const T s = ax / thr;
            return { std::copysign(s, x), T(1) };
        }

        // soft knee region width in “threshold units”
        // kneeWidth = mKnee * thr : 0..thr
        const T kw = mKnee * thr;
        const T a0 = thr - kw; // knee start
        const T a1 = thr + kw; // knee end

        if(ax <= a0){
            // linear region
            return { x / thr, T(0) };
        } else if(ax >= a1){
            // fully nonlinear region
            return { std::copysign(ax / thr, x), T(1) };
        } else {
            // in knee: blend between linear and nonlinear mapping
            const T t = (ax - a0) / (a1 - a0); // 0..1
            const T b = smoothstep01(t);

            const T xLin  = x / thr;
            const T xNL   = std::copysign(ax / thr, x);
            const T xMap  = xLin + (xNL - xLin) * b;
            return { xMap, b };
        }
    }

private:
    T mThr  = T(1);
    T mKnee = T(0);
};

// -----------------------------
// Asymmetry weighting (Strategy A)
// Computes effective x by weighting + and - halves differently.
// asym in [-1,1]: + => more positive drive, - => more negative drive.
// -----------------------------
template<class T>
class AsymmetryWeight {
public:
    void setAsym(T a){ mAsym = clampSym(a); }

    inline T operator()(T x) const {
        // weights in [0,2], centered at 1
        const T wp = T(1) + mAsym;  // when asym=+1 => wp=2, wn=0
        const T wn = T(1) - mAsym;
        return (x >= T(0)) ? (x * wp) : (x * wn);
    }

private:
    T mAsym = T(0);
};

// -----------------------------
// Harmonic enhancer (Option 1): even/odd extraction
// Given a “base shaper” f(x), we can add controlled harmonic emphasis.
// harmonics in [0,1] scales injected harmonics.
// evenOdd: optional tilt between even/odd (not required by your API, but useful)
// -----------------------------
template<class T>
class HarmonicEnhancer {
public:
    void setAmount(T a){ mAmt = clamp01(a); }
    void setEvenOdd(T eo){ mEvenOdd = clampSym(eo); } // -1 odd, +1 even

    template<class Curve>
    inline T apply(T x, const Params<T>& p) const {
        if(mAmt <= T(0)) return T(0);

        // decompose using the curve itself (works best with smooth curves)
        const T fx  = Curve::template f<T>(x, p);
        const T fnx = Curve::template f<T>(-x, p);

        const T odd  = (fx - fnx) * T(0.5);
        const T even = (fx + fnx) * T(0.5);

        // tilt: -1 => all odd, +1 => all even
        const T t = (mEvenOdd + T(1)) * T(0.5); // 0..1
        const T h = odd * (T(1) - t) + even * t;

        // inject only the harmonic portion (h - x'ish)
        // We want “added harmonics” not “replace output”
        return (h - x) * mAmt;
    }

private:
    T mAmt = T(0);
    T mEvenOdd = T(0);
};

// -----------------------------
// Curves
// Each Curve must provide: f(x,p). Optional: F(x,p) later for ADAA.
// x arrives in “normalized shaper domain” (typically threshold-mapped).
// -----------------------------

struct SoftClipTanh {
    template<class T>
    static inline T f(T x, const Params<T>& p){
        // unity slope at 0 if drive is handled outside
        // optional hardness can scale saturation
        const T k = T(1) + p.hardness * T(4);
        return std::tanh(x * k);
    }
};

struct SoftClipRational {
    template<class T>
    static inline T f(T x, const Params<T>& /*p*/){
        // smooth, cheap, but not perfectly unity at 0 unless used as-is
        // for harmonic work, tanh is usually better; this is classic “x/(1+|x|)”
        return x / (T(1) + safeAbs(x));
    }
};

struct HardClip {
    template<class T>
    static inline T f(T x, const Params<T>& /*p*/){
        return std::clamp(x, T(-1), T(1));
    }
};

struct TubeLogCosh {
    template<class T>
    static inline T f(T x, const Params<T>& p){
        // “tanh + logcosh-ish family” (stable and smooth)
        // y = tanh(a*x) with a driven by hardness
        const T a = T(0.8) + p.hardness * T(2.5);
        return std::tanh(a * x);
    }
};

struct DiodeSoft {
    template<class T>
    static inline T f(T x, const Params<T>& p){
        // smooth diode-ish: y = sign(x) * (1 - exp(-k*|x|))
        // k increases with hardness
        const T k = T(1.5) + p.hardness * T(10);
        const T ax = safeAbs(x);
        const T y = T(1) - safeExp(-k * ax);
        return std::copysign(y, x);
    }
};

// -----------------------------
// WaveshaperCore (Static core)
// - no smoothing inside (outer stage should do it)
// - canonical ordering: drive -> bias -> asym mapping -> knee/threshold -> curve -> optional harmonics
// -----------------------------
template<class T, class Curve = SoftClipTanh>
class WaveshaperCore {
public:
    void reset(){ /* stateless core */ }

    // --- API from your plan ---
    void setDrive(T v){ mP.drive = std::max(T(0), v); }
    void setBias(T v){ mP.bias = v; }
    void setAsymmetry(T v){ mP.asym = clampSym(v); mAsymW.setAsym(mP.asym); }
    void setThreshold(T v){ mP.threshold = std::max(T(0), v); mKnee.setThreshold(mP.threshold); }
    void setKnee(T v){ mP.knee = clamp01(v); mKnee.setKnee(mP.knee); }
    void setHarmonics(T v){ mP.harmonics = clamp01(v); mHarm.setAmount(mP.harmonics); }

    // Optional curve-specific knob
    void setHardness(T v){ mP.hardness = clamp01(v); }

    // Optional harmonic tilt (not in your minimal API; leave if you want)
    void setEvenOdd(T v){ mHarm.setEvenOdd(clampSym(v)); }

    inline T operator()(T x){
        // 1) pre gain
        x *= mP.drive;

        // 2) bias
        x += mP.bias;

        // 3) asymmetry weighting (branch weighting)
        x = mAsymW(x);

        // 4) threshold + knee mapping to normalized domain
        //    After mapping, “threshold” corresponds roughly to |x| ~ 1.
        const auto km = mKnee(x);
        T xn = km.x;

        // 5) transfer function
        T y = Curve::template f<T>(xn, mP);

        // 6) optional harmonic injection (post-shape)
        //    We inject harmonics relative to xn (domain signal feeding curve).
        //    If you prefer injection relative to y, change (h - xn) in enhancer.
        if(mP.harmonics > T(0)){
            y += mHarm.template apply<Curve>(xn, mP);
        }

        return y;
    }

    // Direct access if you want to reuse params outside
    const Params<T>& params() const { return mP; }
    Params<T>& params() { return mP; }

private:
    Params<T> mP;

    KneeMapper<T>      mKnee;
    AsymmetryWeight<T> mAsymW;
    HarmonicEnhancer<T> mHarm;
};

} // namespace ws
