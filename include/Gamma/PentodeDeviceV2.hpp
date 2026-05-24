// PentodeZDFStageV2.hpp
#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

// ============================================================================
// Helper math (header-only, no deps)
// ============================================================================
namespace detail {

template<typename T>
static inline T clamp(T v, T lo, T hi) {
    return std::min(hi, std::max(lo, v));
}

template<typename T>
static inline bool isFinite(T v) {
    return std::isfinite(static_cast<double>(v));
}

// softplus(x;Vt) in volts (smooth max(x,0) behavior when used on voltages)
template<typename T>
static inline T softplusV(T x, T Vt) {
    const T s = x / Vt;
    if (s > T(40))  return x;
    if (s < T(-40)) return T(0);
    return Vt * std::log1p(std::exp(s));
}

// smooth rectifier: behaves like max(v,0) but smooth for Newton
template<typename T>
static inline T vpos(T v, T Vt) {
    return softplusV(v, Vt);
}

// d/dv softplus(v,Vt) = sigmoid(v/Vt)
template<typename T>
static inline T d_softplusV(T x, T Vt) {
    const T s = x / Vt;
    if (s > T(40))  return T(1);
    if (s < T(-40)) return T(0);
    const T e = std::exp(s);
    return e / (T(1) + e);
}

} // namespace detail


// ============================================================================
// Newton-friendly PentodeDevice (smooth clamps, screen hogging, grid conduction)
// If you have a better data-fit model, swap this type parameter in the stage.
// ============================================================================
template<typename T>
struct PentodeDeviceV2 {
    static_assert(std::is_floating_point_v<T>, "PentodeDeviceV2<T>: T must be float/double");

    // -------- current scales
    T Kp = T(2e-6);
    T Ks = T(5e-7);

    // -------- knees/exponents
    T Vt = T(0.22);      // smoothing knee (V) for softplus and rectifiers
    T p  = T(1.5);

    // -------- control law
    T aS = T(0.02);
    T V0 = T(1.2);

    // -------- weak plate dependence (pentode flattening)
    T Vp0   = T(50);
    T gamma = T(0.25);

    // -------- screen hogging
    T Vhog = T(40);
    T s0   = T(0.05);
    T s1   = T(0.35);
    T sMax = T(0.60);    // fraction clamp (prevents pathological screen runaway)

    // -------- optional screen soft-floor in Vsk (prevents brittle collapse)
    T VskFloor = T(0);   // set 3..10 V if you want

    // -------- grid conduction
    T Kg  = T(2e-6);
    T Vtg = T(0.1);
    T Vg0 = T(0.0);

    // -------- misc safety
    T iMax = T(0.05);    // absolute current clamp (A) to prevent explosions in edge cases

    static inline T powp(T x, T p) {
        x = std::max(x, T(0));
        return std::pow(x, p);
    }

    // Smooth rectifiers for voltages that should not go negative in the model:
    inline T Vpos_(T v) const { return detail::vpos(v, Vt); }

    // Plate current Ip(Vgk,Vpk,Vsk)
    inline T Ip(T Vgk, T Vpk, T Vsk) const {
        // smooth physical constraints
        const T Vpkp = Vpos_(Vpk);
        T Vskp = Vpos_(Vsk);
        if (VskFloor > T(0)) Vskp = std::max(Vskp, VskFloor);

        // drive law (smooth cutoff)
        const T driveV = Vgk + aS * Vskp - V0;
        const T D = detail::softplusV(driveV, Vt);
        const T P = powp(D, p);

        // weak plate dependence
        const T denom = (Vpkp + Vp0);
        const T r = (denom > T(0)) ? (Vpkp / denom) : T(0);
        const T Fp = (r > T(0)) ? std::pow(r, gamma) : T(0);

        T ip = Kp * P * Fp;
        ip = std::max(ip, T(0));
        if (iMax > T(0)) ip = std::min(ip, iMax);
        return ip;
    }

    // Screen current Is(Vgk,Vpk,Vsk)
    inline T Is(T Vgk, T Vpk, T Vsk) const {
        const T Vpkp = Vpos_(Vpk);
        T Vskp = Vpos_(Vsk);
        if (VskFloor > T(0)) Vskp = std::max(Vskp, VskFloor);

        const T driveV = Vgk + aS * Vskp - V0;
        const T D = detail::softplusV(driveV, Vt);
        const T P = powp(D, p);

        // hog term rises when Vpk is low
        const T inv = (Vhog > T(0)) ? (T(1) / Vhog) : T(0);
        const T hog = std::exp(-Vpkp * inv);

        T frac = s0 + s1 * hog;
        frac = std::min(std::max(frac, T(0)), sMax);

        T is = Ks * P * frac;
        is = std::max(is, T(0));
        if (iMax > T(0)) is = std::min(is, iMax);
        return is;
    }

    // Grid current Ig(Vgk): flows from grid node to cathode when grid is positive
    inline T Ig(T Vgk) const {
        if (Kg <= T(0)) return T(0);
        const T g = detail::softplusV(Vgk - Vg0, Vtg);
        // ~ g^(3/2)
        T ig = Kg * (g * std::sqrt(std::max(g, T(0))));
        ig = std::max(ig, T(0));
        if (iMax > T(0)) ig = std::min(ig, iMax);
        return ig;
    }
};


// ============================================================================
// PentodeZDFStageV2
// 4 unknowns implicit solve (Vg, Vk, Vp, Vs) per sample with Newton + damping.
// Adds:
//   - AC coupled grid input (Cg) + grid leak (Rg) -> blocking distortion
//   - optional grid stopper (Rstop) as a simple limiter on Ig (no grid capacitance)
//   - screen node with Rs + Cs decoupling (ZDF/BE)
//   - cathode Rk // Ck (ZDF/BE)
//   - triode strap constraint: Vs = Vp (replaces screen KCL)
// Robustness:
//   - voltage-scaled FD Jacobian steps
//   - clamps + line search
//   - non-finite kill switch & fallback
// ============================================================================
template<class Tv = gam::real,
         class Td = GAM_DEFAULT_DOMAIN,
         class Model = PentodeDeviceV2<Tv>>
class PentodeZDFStageV2 : public Td {
public:
    using T = Tv;

    PentodeZDFStageV2() { reset(); }

    // ----------------------------
    // Gamma lifecycle
    // ----------------------------
    void reset() {
        // node states
        mVg = T(0);
        mVk = T(0);
        mVp = mBplus;
        mVs = mVsupply;

        // BE states
        mVk_prev = mVk;
        mVs_prev = mVs;

        // coupling cap stores Vc = Vin - Vg
        mVc_prev = T(0);
        mVin_prev = T(0);

        mLastIn = T(0);
        mLastOut = T(0);

        mHadNonFinite = false;
    }

    void onDomainChange(double) {
        mFs = std::max(T(Td::spu()), T(1));
        mTs = T(1) / mFs;

        mGk = (mCk > T(0)) ? (mCk / mTs) : T(0);
        mGs = (mCs > T(0)) ? (mCs / mTs) : T(0);
        mGc = (mCg > T(0)) ? (mCg / mTs) : T(0);
    }

    // ----------------------------
    // Audio / calibration
    // ----------------------------
    void drive(T d) { mDrive = std::max(d, T(0)); }
    void bias(T b)  { mBias = b; }           // input units (pre-cap)
    void trim(T t)  { mTrim = t; }
    void calibration(T refVolts) { mRefV = std::max(refVolts, T(1e-6)); }

    // ----------------------------
    // Circuit parameters
    // ----------------------------
    void bplus(T v)   { mBplus = std::max(v, T(1)); }
    void vsupply(T v) { mVsupply = std::max(v, T(0)); }

    void rp(T ohms) { mRp = std::max(ohms, T(1)); }
    void rk(T ohms) { mRk = std::max(ohms, T(1)); }
    void ck(T farads) {
        mCk = std::max(farads, T(0));
        mGk = (mCk > T(0)) ? (mCk / mTs) : T(0);
    }

    void rs(T ohms) { mRs = std::max(ohms, T(1)); }
    void cs(T farads) {
        mCs = std::max(farads, T(0));
        mGs = (mCs > T(0)) ? (mCs / mTs) : T(0);
    }

    // Grid coupling network
    void rg(T ohms) { mRg = std::max(ohms, T(1)); }
    void cg(T farads) {
        mCg = std::max(farads, T(0));
        mGc = (mCg > T(0)) ? (mCg / mTs) : T(0);
    }

    // Grid stopper (simple Ig limiter, since we don't model grid capacitance here)
    void gridStop(T ohms) { mRstop = std::max(ohms, T(0)); }

    // Triode strap: enforce Vs = Vp (constraint equation)
    void triodeStrap(bool e) { mTriodeStrap = e; }

    // Grid conduction enable
    void enableGridConduction(bool e) { mGridConduction = e; }

    // ----------------------------
    // Solver knobs
    // ----------------------------
    void newtonIters(int n) { mIters = std::clamp(n, 1, 24); }
    void newtonTol(T t)     { mTol = std::max(t, T(1e-12)); }
    void fdStepVolts(T h)   { mH = std::max(h, T(1e-9)); }
    void maxLineSearch(int n) { mLineSearch = std::clamp(n, 2, 12); }

    // Kill switch / fallback behavior
    void hardClampOnFailure(bool e) { mHardClampOnFailure = e; }

    // ----------------------------
    // Model access
    // ----------------------------
    Model& model() { return mModel; }
    const Model& model() const { return mModel; }

    // ----------------------------
    // Processing
    // ----------------------------
    inline T operator()(T x) {
        // Vin (volts) from input source
        const T Vin = (x * mDrive + mBias) * mRefV;

        solveNewton_(Vin);

        // Output: plate centered around B+/2 (same convention as your triode stage)
        const T vout = (mVp - T(0.5) * mBplus);
        const T y = (vout / mRefV) * mTrim;

        mLastIn = x;
        mLastOut = y;
        return y;
    }

    // Debug taps
    T gridV() const { return mVg; }
    T cathodeV() const { return mVk; }
    T plateV() const { return mVp; }
    T screenV() const { return mVs; }
    bool hadNonFinite() const { return mHadNonFinite; }

private:
    // ------------------------------------------------------------------------
    // Grid current with optional grid stopper limiting.
    // We model grid stopper as limiting the current drawn from the grid node:
    // Ig_eff = min(Ig_raw, max(0, (Vg - Vk)/Rstop) ) when Rstop>0.
    // This is a pragmatic limiter; a full model needs grid capacitance.
    // ------------------------------------------------------------------------
    inline T gridCurrentEff_(T Vg, T Vk) const {
        if (!mGridConduction) return T(0);

        const T Vgk = Vg - Vk;
        const T Ig_raw = mModel.Ig(Vgk);

        if (mRstop <= T(0)) return Ig_raw;

        // limit by Ohm's law through stopper from grid node toward cathode
        const T Ig_lim = std::max(T(0), Vgk / mRstop);
        return std::min(Ig_raw, Ig_lim);
    }

    // ------------------------------------------------------------------------
    // Residuals: unknowns are (Vg, Vk, Vp, Vs)
    //
    // Grid node:
    //   fG = Vg/Rg + Ig_eff - iCg = 0
    //   iCg = Gc * ((Vin - Vg) - Vc_prev)
    //
    // Cathode node:
    //   fK = Vk/Rk + iCk - (Ip + Is + Ig_eff) = 0
    //   iCk = Gk*(Vk - Vk_prev)
    //
    // Plate node:
    //   fP = (B+ - Vp)/Rp - Ip = 0
    //
    // Screen node:
    //   if triodeStrap: fS = Vs - Vp = 0
    //   else: fS = (Vsupply - Vs)/Rs - Is - iCs = 0
    //         iCs = Gs*(Vs - Vs_prev)
    // ------------------------------------------------------------------------
    inline void residuals_(T Vin, T Vg, T Vk, T Vp, T Vs,
                           T& fG, T& fK, T& fP, T& fS) const
    {
        // device voltages
        const T Vgk = Vg - Vk;
        const T Vpk = Vp - Vk;
        const T Vsk = Vs - Vk;

        const T Ip = mModel.Ip(Vgk, Vpk, Vsk);
        const T Is = mModel.Is(Vgk, Vpk, Vsk);
        const T Ig = gridCurrentEff_(Vg, Vk);

        // coupling cap current Vin->Vg (BE, state is Vc_prev = Vin_prev - Vg_prev)
        const T iCg = (mGc > T(0)) ? (mGc * ((Vin - Vg) - mVc_prev)) : T(0);

        // grid leak
        const T iRg = Vg / mRg;

        // grid KCL
        fG = iRg + Ig - iCg;

        // cathode elements
        const T iRk = Vk / mRk;
        const T iCk = (mGk > T(0)) ? (mGk * (Vk - mVk_prev)) : T(0);

        // cathode KCL
        fK = iRk + iCk - (Ip + Is + Ig);

        // plate resistor current
        const T iRp = (mBplus - Vp) / mRp;
        fP = iRp - Ip;

        // screen
        if (mTriodeStrap) {
            fS = Vs - Vp; // constraint equation replaces screen KCL
        } else {
            const T iRs = (mVsupply - Vs) / mRs;
            const T iCs = (mGs > T(0)) ? (mGs * (Vs - mVs_prev)) : T(0);
            fS = iRs - Is - iCs;
        }
    }

    // ------------------------------------------------------------------------
    // Finite-difference Jacobian (4x4), with voltage-scaled steps
    // J_ij = d f_i / d u_j, u = [Vg, Vk, Vp, Vs]
    // ------------------------------------------------------------------------
    inline void jacobianFD_(T Vin, T Vg, T Vk, T Vp, T Vs,
                            T J[4][4], T f[4]) const
    {
        residuals_(Vin, Vg, Vk, Vp, Vs, f[0], f[1], f[2], f[3]);

        const T hG = mH * (T(1) + std::abs(Vg));
        const T hK = mH * (T(1) + std::abs(Vk));
        const T hP = mH * (T(1) + std::abs(Vp));
        const T hS = mH * (T(1) + std::abs(Vs));

        // d/dVg
        {
            T fp[4];
            residuals_(Vin, Vg + hG, Vk, Vp, Vs, fp[0], fp[1], fp[2], fp[3]);
            for (int i=0;i<4;i++) J[i][0] = (fp[i] - f[i]) / hG;
        }
        // d/dVk
        {
            T fp[4];
            residuals_(Vin, Vg, Vk + hK, Vp, Vs, fp[0], fp[1], fp[2], fp[3]);
            for (int i=0;i<4;i++) J[i][1] = (fp[i] - f[i]) / hK;
        }
        // d/dVp
        {
            T fp[4];
            residuals_(Vin, Vg, Vk, Vp + hP, Vs, fp[0], fp[1], fp[2], fp[3]);
            for (int i=0;i<4;i++) J[i][2] = (fp[i] - f[i]) / hP;
        }
        // d/dVs
        {
            T fp[4];
            residuals_(Vin, Vg, Vk, Vp, Vs + hS, fp[0], fp[1], fp[2], fp[3]);
            for (int i=0;i<4;i++) J[i][3] = (fp[i] - f[i]) / hS;
        }
    }

    // ------------------------------------------------------------------------
    // Solve linear system A x = b (4x4) using Gaussian elimination w/ pivoting
    // ------------------------------------------------------------------------
    static inline bool solve4x4_(T A[4][4], T b[4], T x[4], T eps) {
        // Forward elimination
        for (int k=0; k<4; ++k) {
            // pivot
            int piv = k;
            T vmax = std::abs(A[k][k]);
            for (int i=k+1; i<4; ++i) {
                T v = std::abs(A[i][k]);
                if (v > vmax) { vmax = v; piv = i; }
            }
            if (vmax < eps) return false;

            if (piv != k) {
                for (int j=k; j<4; ++j) std::swap(A[k][j], A[piv][j]);
                std::swap(b[k], b[piv]);
            }

            const T inv = T(1) / A[k][k];
            for (int i=k+1; i<4; ++i) {
                const T m = A[i][k] * inv;
                A[i][k] = T(0);
                for (int j=k+1; j<4; ++j) A[i][j] -= m * A[k][j];
                b[i] -= m * b[k];
            }
        }

        // Back substitution
        for (int i=3; i>=0; --i) {
            T s = b[i];
            for (int j=i+1; j<4; ++j) s -= A[i][j] * x[j];
            x[i] = s / A[i][i];
        }
        return true;
    }

    // ------------------------------------------------------------------------
    // Newton solve with damping & clamps; updates BE states on commit
    // ------------------------------------------------------------------------
    void solveNewton_(T Vin) {
        // Warm start
        T Vg = mVg;
        T Vk = mVk;
        T Vp = mVp;
        T Vs = mTriodeStrap ? mVp : mVs;

        // Physical-ish clamps
        const T VgMin = T(-80);
        const T VgMax = T( 80);
        const T VkMin = T(-80);
        const T VkMax = std::max(mBplus, mVsupply) + T(20);
        const T VpMin = T(0);
        const T VpMax = mBplus;
        const T VsMin = T(0);
        const T VsMax = mVsupply;

        // Save last good state for fallback
        const T Vg0 = Vg, Vk0 = Vk, Vp0 = Vp, Vs0 = Vs;

        bool ok = true;

        for (int it=0; it<mIters; ++it) {
            if (mTriodeStrap) Vs = Vp;

            T J[4][4];
            T f[4];
            jacobianFD_(Vin, Vg, Vk, Vp, Vs, J, f);

            // non-finite check
            for(int i=0;i<4;i++){
                if(!detail::isFinite(f[i])) { ok = false; break; }
                for(int j=0;j<4;j++){
                    if(!detail::isFinite(J[i][j])) { ok = false; break; }
                }
                if(!ok) break;
            }
            if(!ok) break;

            const T err = std::abs(f[0]) + std::abs(f[1]) + std::abs(f[2]) + std::abs(f[3]);
            if (err < mTol) break;

            // Build A and rhs
            T A[4][4];
            for(int i=0;i<4;i++) for(int j=0;j<4;j++) A[i][j] = J[i][j];

            T b[4] = { -f[0], -f[1], -f[2], -f[3] };
            T d[4] = { T(0), T(0), T(0), T(0) };

            if (!solve4x4_(A, b, d, T(1e-18))) { ok = false; break; }

            // line search damping
            T lambda = T(1);
            bool accepted = false;

            for (int ls=0; ls<mLineSearch; ++ls) {
                const T VgTry = detail::clamp(Vg + lambda*d[0], VgMin, VgMax);
                const T VkTry = detail::clamp(Vk + lambda*d[1], VkMin, VkMax);
                const T VpTry = detail::clamp(Vp + lambda*d[2], VpMin, VpMax);
                const T VsTry = mTriodeStrap ? VpTry : detail::clamp(Vs + lambda*d[3], VsMin, VsMax);

                T ft[4];
                residuals_(Vin, VgTry, VkTry, VpTry, VsTry, ft[0], ft[1], ft[2], ft[3]);

                if(!detail::isFinite(ft[0]) || !detail::isFinite(ft[1]) ||
                   !detail::isFinite(ft[2]) || !detail::isFinite(ft[3])) {
                    lambda *= T(0.5);
                    continue;
                }

                const T errTry = std::abs(ft[0]) + std::abs(ft[1]) + std::abs(ft[2]) + std::abs(ft[3]);
                if (errTry <= err) {
                    Vg = VgTry; Vk = VkTry; Vp = VpTry; Vs = VsTry;
                    accepted = true;
                    break;
                }
                lambda *= T(0.5);
            }

            if (!accepted) {
                // Couldn't improve -> stop
                ok = false;
                break;
            }
        }

        // Commit or fallback
        if (!ok) {
            mHadNonFinite = true;

            // fallback to last state
            Vg = Vg0; Vk = Vk0; Vp = Vp0; Vs = Vs0;

            // optional hard clamp into a plausible safe point
            if (mHardClampOnFailure) {
                Vg = detail::clamp(Vg, VgMin, VgMax);
                Vk = detail::clamp(Vk, VkMin, VkMax);
                Vp = detail::clamp(Vp, VpMin, VpMax);
                Vs = mTriodeStrap ? Vp : detail::clamp(Vs, VsMin, VsMax);
            }
        }

        if (mTriodeStrap) Vs = Vp;

        // store node values
        mVg = Vg;
        mVk = Vk;
        mVp = Vp;
        mVs = Vs;

        // Update BE states after commit:
        mVk_prev = mVk;
        mVs_prev = mVs;

        // Coupling cap state stores Vc = Vin - Vg (previous step)
        mVc_prev = (Vin - mVg);
        mVin_prev = Vin;
    }

private:
    // sample rate
    T mFs { T(48000) };
    T mTs { T(1) / T(48000) };

    // audio params
    T mDrive { T(1) };
    T mBias  { T(0) };
    T mTrim  { T(1) };
    T mRefV  { T(2) };

    // circuit params
    T mBplus   { T(250) };
    T mVsupply { T(250) };

    T mRp { T(100000) };
    T mRk { T(1500) };
    T mCk { T(1e-6) };

    T mRs { T(47000) };
    T mCs { T(4.7e-6) };

    // grid coupling
    T mRg { T(1e6) };
    T mCg { T(22e-9) };
    T mRstop { T(1000) };   // ~1k grid stopper default (pragmatic limiter)

    // BE conductances
    T mGk { T(0) };
    T mGs { T(0) };
    T mGc { T(0) };

    // mode / options
    bool mTriodeStrap { false };
    bool mGridConduction { true };
    bool mHardClampOnFailure { true };

    // solver
    int mIters { 10 };
    int mLineSearch { 8 };
    T mTol { T(1e-9) };
    T mH   { T(1e-6) };

    // device model
    Model mModel;

    // node states
    T mVg { T(0) };
    T mVk { T(0) };
    T mVp { T(0) };
    T mVs { T(0) };

    // BE states
    T mVk_prev { T(0) };
    T mVs_prev { T(0) };
    T mVc_prev { T(0) };   // coupling cap state: Vin - Vg previous
    T mVin_prev { T(0) };

    // debug
    T mLastIn  { T(0) };
    T mLastOut { T(0) };
    bool mHadNonFinite { false };
};

} // namespace gam
