#pragma once
#include <algorithm>
#include <cmath>
#include <type_traits>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

// -----------------------------------------------------------------------------
// NewtonZDF<2,T> (Phase 1.1 solver) — include from wherever you placed it.
// If you put it under gdsp/NewtonZDF.h as suggested, include that.
// -----------------------------------------------------------------------------
#include "NewtonZDF.h"

namespace gam {

// -----------------------------------------------------------------------------
// Triode device core (Koren-ish static model). If you already have your own
// Triode<T> core, swap this out and keep the stage unchanged.
// -----------------------------------------------------------------------------
template<typename T>
struct TriodeCore {
    static_assert(std::is_floating_point_v<T>, "TriodeCore<T>: T must be float/double");

    // "physical-ish" params
    T mu  = T(100);
    T kg1 = T(1060);
    T kp  = T(600);
    T ex  = T(1.4);
    T vct = T(0.0);

    // stabilizers
    T driveSat     = T(0.7);
    T cutoffSmooth = T(0.02); // 0 => hard clamp; >0 => smooth clamp
    T iMax         = T(10);

    T eFloor   = T(1e-6);
    T vpkFloor = T(0.05);

    static inline T softClamp0(T x, T s){
        return T(0.5) * (x + std::sqrt(x*x + s*s));
    }

    inline T operator()(T vgk, T vpk) const {
        T e = vgk + vct + vpk / mu;

        e = (cutoffSmooth > T(0)) ? softClamp0(e, cutoffSmooth)
                                  : std::max(e, T(0));

        if (driveSat > T(0)) e = std::tanh(driveSat * e);

        e = std::max(e, eFloor);

        const T vpkPos = std::max(vpk, vpkFloor);
        const T denom  = T(1) + kp * vpkPos;

        T ip = kg1 * std::pow(e, ex) / denom;

        ip = std::max(ip, T(0));
        ip = std::min(ip, iMax);
        return ip;
    }
};

// -----------------------------------------------------------------------------
// TriodeZDFStage: 2-unknown implicit/ZDF solve (Vk, Vp)
// Topology: B+--Rp--Vp--triode-->Vk--Rk//Ck--GND
// Optional grid conduction Ig(Vgk) adds realism/crunch.
// Uses NewtonZDF<2,T> from Phase 1.1.
// -----------------------------------------------------------------------------
template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class TriodeZDFStage : public Td {
public:
    using T = Tv;

    TriodeZDFStage(){
        reset();
    }

    // ----------------------------
    // Gamma lifecycle
    // ----------------------------
    void reset(){
        mVk_z = T(0);
        mVk   = T(0);
        mVp   = mBplus;

        mLastIn  = T(0);
        mLastOut = T(0);
    }

    void onDomainChange(double){
        mFs = std::max(T(Td::spu()), T(1));
        mTs = T(1) / mFs;

        // backward Euler conductance for Ck
        mGk = (mCk > T(0)) ? (mCk / mTs) : T(0);
    }

    // ----------------------------
    // Audio / calibration
    // ----------------------------
    void drive(T d){ mDrive = std::max(d, T(0)); }
    void bias(T b){ mBias = b; }              // input units
    void trim(T t){ mTrim = t; }
    void calibration(T refVolts){ mRefV = std::max(refVolts, T(1e-6)); } // volts per input unit

    // ----------------------------
    // Circuit parameters
    // ----------------------------
    void bplus(T v){ mBplus = std::max(v, T(1)); }
    void rp(T ohms){ mRp = std::max(ohms, T(1)); }
    void rk(T ohms){ mRk = std::max(ohms, T(1)); }
    void ck(T farads){
        mCk = std::max(farads, T(0));
        mGk = (mCk > T(0)) ? (mCk / mTs) : T(0);
    }

    // ----------------------------
    // Grid conduction controls
    // ----------------------------
    void enableGridConduction(bool e){ mGridConduction = e; }
    void gridG(T G){ mGg = std::max(G, T(0)); }
    void gridVt(T vt){ mVtg = std::max(vt, T(1e-6)); }
    void gridV0(T v0){ mVg0 = v0; }

    // ----------------------------
    // Solver knobs (pass-through to NewtonZDF)
    // ----------------------------
    void newtonIters(int n){ mNewton.s.iters = std::clamp(n, 1, 16); }
    void newtonTol(T t){ mNewton.s.tol = std::max(t, T(1e-12)); }
    void fdStepVolts(T h){ mNewton.s.fdStep = std::max(h, T(1e-9)); }

    // Optional: line search halvings (default 6)
    void lineSearchSteps(int n){ mNewton.s.lineSearch = std::clamp(n, 0, 12); }

    // ----------------------------
    // Device core access
    // ----------------------------
    TriodeCore<T>& core(){ return mCore; }
    const TriodeCore<T>& core() const { return mCore; }

    // ----------------------------
    // Processing
    // ----------------------------
    inline T operator()(T x){
        // input -> grid voltage (cathode referenced)
        const T Vg = (x * mDrive + mBias) * mRefV;

        solve_(Vg);

        // output: inverted plate swing, centered around B+/2
        const T vout = (mVp - T(0.5) * mBplus);
        const T y = (vout / mRefV) * mTrim;

        mLastIn = x;
        mLastOut = y;
        return y;
    }

    // Debug taps
    T plateV() const { return mVp; }
    T cathodeV() const { return mVk; }
    T lastIn() const { return mLastIn; }
    T lastOut() const { return mLastOut; }

private:
    static inline T clamp_(T v, T lo, T hi){ return std::min(hi, std::max(lo, v)); }

    static inline T softplusV_(T x, T Vt){
        const T s = x / Vt;
        if (s > T(40))  return x;
        if (s < T(-40)) return T(0);
        return Vt * std::log1p(std::exp(s));
    }

    inline T gridCurrent_(T Vgk) const {
        if (!mGridConduction || mGg <= T(0)) return T(0);
        const T g = softplusV_(Vgk - mVg0, mVtg);
        // ~3/2 conduction
        return mGg * (g * std::sqrt(std::max(g, T(0))));
    }

    // Residual functor for NewtonZDF<2,T>
    struct Residual {
        T Vg;
        T Bplus, Rp, Rk, Gk, Vk_z;
        const TriodeCore<T>& triode;

        bool gridOn;
        T Gg, Vtg, Vg0;

        static inline T softplusV(T x, T Vt){
            const T s = x / Vt;
            if (s > T(40))  return x;
            if (s < T(-40)) return T(0);
            return Vt * std::log1p(std::exp(s));
        }

        inline T Ig(T Vgk) const {
            if (!gridOn || Gg <= T(0)) return T(0);
            const T g = softplusV(Vgk - Vg0, Vtg);
            return Gg * (g * std::sqrt(std::max(g, T(0))));
        }

        // x[0]=Vk, x[1]=Vp
        inline void operator()(const T x[2], T f[2]) const {
            const T Vk = x[0];
            const T Vp = x[1];

            const T Vgk = Vg - Vk;
            const T Vpk = Vp - Vk;

            const T Ip  = triode(Vgk, Vpk);
            const T Ig_ = Ig(Vgk);

            const T iRk = Vk / Rk;
            const T iCk = (Gk > T(0)) ? (Gk * (Vk - Vk_z)) : T(0);
            const T iRp = (Bplus - Vp) / Rp;

            // Cathode KCL and Plate KCL
            f[0] = iRk + iCk - (Ip + Ig_);
            f[1] = iRp - Ip;
        }
    };

    void solve_(T Vg){
        // Unknowns: x = [Vk, Vp]
        T x[2] = { mVk, mVp };

        // Bounds
        const T lo[2] = { T(-50),  T(0) };
        const T hi[2] = { mBplus,  mBplus };

        Residual r{
            Vg,
            mBplus, mRp, mRk, mGk, mVk_z,
            mCore,
            mGridConduction, mGg, mVtg, mVg0
        };

        // Solve
        (void)mNewton.solve(x, lo, hi, r);

        // Commit
        mVk = x[0];
        mVp = x[1];

        // Backward Euler state update
        mVk_z = mVk;
    }

private:
    // sample rate
    T mFs { T(48000) };
    T mTs { T(1) / T(48000) };

    // audio params
    T mDrive { T(1) };
    T mBias  { T(0) };
    T mTrim  { T(1) };
    T mRefV  { T(2) }; // volts per input unit

    // circuit params
    T mBplus { T(250) };
    T mRp    { T(100000) };
    T mRk    { T(1500) };
    T mCk    { T(1e-6) };
    T mGk    { T(0) };   // Ck/T

    // grid conduction
    bool mGridConduction { true };
    T mGg  { T(2e-6) };
    T mVtg { T(0.1) };
    T mVg0 { T(0.0) };

    // device
    TriodeCore<T> mCore;

    // solver
    gdsp::NewtonZDF<2, T> mNewton;

    // states
    T mVk_z { T(0) };
    T mVk   { T(0) };
    T mVp   { T(0) };

    // debug
    T mLastIn  { T(0) };
    T mLastOut { T(0) };
};

} // namespace gam
