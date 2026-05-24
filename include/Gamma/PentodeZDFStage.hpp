#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

// -----------------------------------------------------------------------------
// Default pentode device model: Ip(Vgk,Vpk,Vsk), Is(...), Ig(Vgk)
// Smooth, Newton-friendly, pentode-ish (flat plate curves + screen hogging).
// Swap this out later for Koren/data-fit without touching the stage code.
// -----------------------------------------------------------------------------
template<typename T>
struct PentodeDevice {
    static_assert(std::is_floating_point_v<T>, "PentodeDevice<T>: T must be float/double");

    // current scales
    T Kp = T(2e-6);
    T Ks = T(5e-7);

    // cutoff knee (V)
    T Vt = T(0.18);

    // power exponent
    T p  = T(1.5);

    // screen influence in control law
    T aS = T(0.02);

    // cutoff shift (V)
    T V0 = T(1.2);

    // plate dependence (weak)
    T Vp0   = T(50);
    T gamma = T(0.25);

    // screen hogging
    T Vhog = T(40);
    T s0   = T(0.05);
    T s1   = T(0.35);

    // grid conduction
    T Kg  = T(2e-6);
    T Vtg = T(0.1);
    T Vg0 = T(0.0);

    static inline T softplusV(T x, T Vt){
        const T s = x / Vt;
        if (s > T(40))  return x;
        if (s < T(-40)) return T(0);
        return Vt * std::log1p(std::exp(s));
    }

    static inline T powp(T x, T p){
        x = std::max(x, T(0));
        return std::pow(x, p);
    }

    // Plate current
    inline T Ip(T Vgk, T Vpk, T Vsk) const {
        Vpk = std::max(Vpk, T(0));
        Vsk = std::max(Vsk, T(0));

        const T driveV = Vgk + aS * Vsk - V0;
        const T D = softplusV(driveV, Vt);
        const T P = powp(D, p);

        // weak plate dependence (pentode trait)
        const T Fp = std::pow(Vpk / (Vpk + Vp0), gamma);

        return Kp * P * Fp;
    }

    // Screen current
    inline T Is(T Vgk, T Vpk, T Vsk) const {
        Vpk = std::max(Vpk, T(0));
        Vsk = std::max(Vsk, T(0));

        const T driveV = Vgk + aS * Vsk - V0;
        const T D = softplusV(driveV, Vt);
        const T P = powp(D, p);

        const T hog  = std::exp(-Vpk / Vhog);     // larger when plate voltage is low
        const T frac = s0 + s1 * hog;

        return Ks * P * frac;
    }

    // Grid current (optional conduction)
    inline T Ig(T Vgk) const {
        if (Kg <= T(0)) return T(0);
        const T g = softplusV(Vgk - Vg0, Vtg);
        // ~3/2 law
        return Kg * (g * std::sqrt(std::max(g, T(0))));
    }
};


// -----------------------------------------------------------------------------
// PentodeZDFStage: preamp pentode stage with dynamic screen node
//
// Unknowns per sample: Vk (cathode), Vp (plate), Vs (screen)
//
// Topology:
//   Plate:   B+  --Rp--  Vp
//   Screen:  Vsupply --Rs-- Vs --Cs-- GND
//   Cathode: Vk --Rk-- GND, Vk --Ck-- GND
//   Device currents flow into cathode: Ip + Is + Ig
//
// ZDF (Backward Euler):
//   Cathode cap: Gk = Ck/T with state Vk_z
//   Screen cap : Gs = Cs/T with state Vs_z
// -----------------------------------------------------------------------------
template<class Tv = gam::real,
         class Td = GAM_DEFAULT_DOMAIN,
         class Model = PentodeDevice<Tv>>
class PentodeZDFStage : public Td {
public:
    using T = Tv;

    PentodeZDFStage(){
        reset();
    }

    // ----------------------------
    // Gamma lifecycle
    // ----------------------------
    void reset(){
        mVk_z = T(0);
        mVs_z = mVsupply;  // screen cap state tends toward supply
        mVk   = T(0);
        mVp   = mBplus;
        mVs   = mVsupply;

        mLastIn  = T(0);
        mLastOut = T(0);
    }

    void onDomainChange(double){
        mFs = std::max(T(Td::spu()), T(1));
        mTs = T(1) / mFs;
        mGk = (mCk > T(0)) ? (mCk / mTs) : T(0);
        mGs = (mCs > T(0)) ? (mCs / mTs) : T(0);
    }

    // ----------------------------
    // Audio / calibration
    // ----------------------------
    void drive(T d){ mDrive = std::max(d, T(0)); }
    void bias(T b){ mBias = b; }                // input units
    void trim(T t){ mTrim = t; }
    void calibration(T refVolts){ mRefV = std::max(refVolts, T(1e-6)); }

    // ----------------------------
    // Circuit parameters
    // ----------------------------
    void bplus(T v){ mBplus = std::max(v, T(1)); }
    void vsupply(T v){ mVsupply = std::max(v, T(0)); }
    void rp(T ohms){ mRp = std::max(ohms, T(1)); }
    void rk(T ohms){ mRk = std::max(ohms, T(1)); }

    void ck(T farads){
        mCk = std::max(farads, T(0));
        mGk = (mCk > T(0)) ? (mCk / mTs) : T(0);
    }

    void rs(T ohms){ mRs = std::max(ohms, T(1)); }

    void cs(T farads){
        mCs = std::max(farads, T(0));
        mGs = (mCs > T(0)) ? (mCs / mTs) : T(0);
    }

    // Triode strap mode: Vs forced to Vp (screen tied to plate)
    void triodeStrap(bool e){ mTriodeStrap = e; }

    // ----------------------------
    // Solver knobs
    // ----------------------------
    void newtonIters(int n){ mIters = std::clamp(n, 1, 16); }
    void newtonTol(T t){ mTol = std::max(t, T(1e-12)); }
    void fdStepVolts(T h){ mH = std::max(h, T(1e-9)); }

    // ----------------------------
    // Model access
    // ----------------------------
    Model& model(){ return mModel; }
    const Model& model() const { return mModel; }

    // ----------------------------
    // Processing
    // ----------------------------
    inline T operator()(T x){
        // grid voltage from input
        const T Vg = (x * mDrive + mBias) * mRefV;

        solveNewton_(Vg);

        // output mapping: plate centered around B+/2
        const T vout = (mVp - T(0.5) * mBplus);
        const T y = (vout / mRefV) * mTrim;

        mLastIn = x;
        mLastOut = y;
        return y;
    }

    // Debug taps
    T plateV() const { return mVp; }
    T screenV() const { return mVs; }
    T cathodeV() const { return mVk; }
    T lastIn() const { return mLastIn; }
    T lastOut() const { return mLastOut; }

private:
    static inline T clamp(T v, T lo, T hi){ return std::min(hi, std::max(lo, v)); }

    // f1: cathode KCL
    // Vk/Rk + Gk*(Vk - Vk_z) - (Ip + Is + Ig) = 0
    // f2: plate KCL
    // (B+ - Vp)/Rp - Ip = 0
    // f3: screen KCL
    // (Vsupply - Vs)/Rs - Is - Gs*(Vs - Vs_z) = 0
    inline void residuals_(T Vg, T Vk, T Vp, T Vs, T& f1, T& f2, T& f3) const {
        const T Vgk = Vg - Vk;
        const T Vpk = Vp - Vk;
        const T Vsk = Vs - Vk;

        const T Ip = mModel.Ip(Vgk, Vpk, Vsk);
        const T Is = mModel.Is(Vgk, Vpk, Vsk);
        const T Ig = mModel.Ig(Vgk);

        const T iRk = Vk / mRk;
        const T iCk = (mGk > T(0)) ? (mGk * (Vk - mVk_z)) : T(0);

        const T iRp = (mBplus - Vp) / mRp;

        const T iRs = (mVsupply - Vs) / mRs;
        const T iCs = (mGs > T(0)) ? (mGs * (Vs - mVs_z)) : T(0);

        f1 = iRk + iCk - (Ip + Is + Ig);
        f2 = iRp - Ip;
        f3 = iRs - Is - iCs;
    }

    // Finite-difference Jacobian 3x3
    inline void jacobianFD_(T Vg, T Vk, T Vp, T Vs,
                            T J[3][3], T f[3]) const
    {
        residuals_(Vg, Vk, Vp, Vs, f[0], f[1], f[2]);

        // d/dVk
        {
            T fp[3];
            residuals_(Vg, Vk + mH, Vp, Vs, fp[0], fp[1], fp[2]);
            J[0][0] = (fp[0] - f[0]) / mH;
            J[1][0] = (fp[1] - f[1]) / mH;
            J[2][0] = (fp[2] - f[2]) / mH;
        }
        // d/dVp
        {
            T fp[3];
            residuals_(Vg, Vk, Vp + mH, Vs, fp[0], fp[1], fp[2]);
            J[0][1] = (fp[0] - f[0]) / mH;
            J[1][1] = (fp[1] - f[1]) / mH;
            J[2][1] = (fp[2] - f[2]) / mH;
        }
        // d/dVs
        {
            T fp[3];
            residuals_(Vg, Vk, Vp, Vs + mH, fp[0], fp[1], fp[2]);
            J[0][2] = (fp[0] - f[0]) / mH;
            J[1][2] = (fp[1] - f[1]) / mH;
            J[2][2] = (fp[2] - f[2]) / mH;
        }
    }

    // Solve 3x3 linear system A x = b using Gaussian elimination with partial pivoting.
    static inline bool solve3x3_(T A[3][3], T b[3], T x[3], T eps){
        // Augment matrix [A|b]
        int p0 = 0, p1 = 1, p2 = 2;

        auto absA = [&](int r, int c)->T { return std::abs(A[r][c]); };

        // pivot column 0
        if (absA(1,0) > absA(p0,0)) p0 = 1;
        if (absA(2,0) > absA(p0,0)) p0 = 2;
        if (std::abs(A[p0][0]) < eps) return false;
        if (p0 != 0) {
            for(int c=0;c<3;++c) std::swap(A[0][c], A[p0][c]);
            std::swap(b[0], b[p0]);
        }

        // eliminate below row 0
        for (int r=1; r<3; ++r) {
            const T m = A[r][0] / A[0][0];
            for (int c=0; c<3; ++c) A[r][c] -= m * A[0][c];
            b[r] -= m * b[0];
        }

        // pivot column 1 (rows 1..2)
        int piv = 1;
        if (std::abs(A[2][1]) > std::abs(A[1][1])) piv = 2;
        if (std::abs(A[piv][1]) < eps) return false;
        if (piv != 1) {
            for(int c=0;c<3;++c) std::swap(A[1][c], A[piv][c]);
            std::swap(b[1], b[piv]);
        }

        // eliminate row 2
        {
            const T m = A[2][1] / A[1][1];
            for (int c=1; c<3; ++c) A[2][c] -= m * A[1][c];
            b[2] -= m * b[1];
        }

        if (std::abs(A[2][2]) < eps) return false;

        // back-substitution
        x[2] = b[2] / A[2][2];
        x[1] = (b[1] - A[1][2]*x[2]) / A[1][1];
        x[0] = (b[0] - A[0][1]*x[1] - A[0][2]*x[2]) / A[0][0];
        return true;
    }

    void solveNewton_(T Vg){
        // Initial guess = last solution
        T Vk = mVk;
        T Vp = mVp;
        T Vs = mTriodeStrap ? mVp : mVs;

        // clamps
        const T VkMin = T(-50);
        const T VkMax = std::max(mBplus, mVsupply);
        const T VpMin = T(0);
        const T VpMax = mBplus;
        const T VsMin = T(0);
        const T VsMax = mVsupply;

        for (int it = 0; it < mIters; ++it) {
            // enforce triode strap constraint before evaluation
            if (mTriodeStrap) Vs = Vp;

            T J[3][3];
            T f[3];
            jacobianFD_(Vg, Vk, Vp, Vs, J, f);

            const T err = std::abs(f[0]) + std::abs(f[1]) + std::abs(f[2]);
            if (err < mTol) break;

            // Newton step: J * d = -f
            T A[3][3] = {
                { J[0][0], J[0][1], J[0][2] },
                { J[1][0], J[1][1], J[1][2] },
                { J[2][0], J[2][1], J[2][2] }
            };
            T b[3] = { -f[0], -f[1], -f[2] };
            T d[3];

            if (!solve3x3_(A, b, d, T(1e-18))) break;

            // line search / damping
            T lambda = T(1);
            for (int ls = 0; ls < 6; ++ls) {
                T VkTry = clamp(Vk + lambda * d[0], VkMin, VkMax);
                T VpTry = clamp(Vp + lambda * d[1], VpMin, VpMax);
                T VsTry = mTriodeStrap ? VpTry : clamp(Vs + lambda * d[2], VsMin, VsMax);

                T f1t, f2t, f3t;
                residuals_(Vg, VkTry, VpTry, VsTry, f1t, f2t, f3t);
                const T errTry = std::abs(f1t) + std::abs(f2t) + std::abs(f3t);

                if (errTry <= err) {
                    Vk = VkTry;
                    Vp = VpTry;
                    Vs = VsTry;
                    break;
                }
                lambda *= T(0.5);
            }
        }

        // commit and update BE states
        if (mTriodeStrap) Vs = Vp;

        mVk = Vk;
        mVp = Vp;
        mVs = Vs;

        mVk_z = Vk;
        mVs_z = Vs;
    }

private:
    // sample rate
    T mFs { T(48000) };
    T mTs { T(1) / T(48000) };

    // audio params
    T mDrive { T(1) };
    T mBias  { T(0) };
    T mTrim  { T(1) };
    T mRefV  { T(2) };     // volts per input unit

    // circuit params
    T mBplus   { T(250) };
    T mVsupply { T(250) };
    T mRp      { T(100000) };
    T mRk      { T(1500) };
    T mCk      { T(1e-6) };
    T mRs      { T(47000) };
    T mCs      { T(4.7e-6) };

    // BE conductances
    T mGk { T(0) };
    T mGs { T(0) };

    // solver params
    int mIters { 8 };
    T   mTol   { T(1e-9) };
    T   mH     { T(1e-6) }; // FD step (volts)

    // mode
    bool mTriodeStrap { false };

    // model
    Model mModel;

    // states
    T mVk_z { T(0) };
    T mVs_z { T(0) };
    T mVk   { T(0) };
    T mVp   { T(0) };
    T mVs   { T(0) };

    // debug
    T mLastIn  { T(0) };
    T mLastOut { T(0) };
};

} // namespace gam
