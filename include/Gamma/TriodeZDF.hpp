#pragma once
#include <cmath>
#include <algorithm>
#include <type_traits>

namespace gdsp {

template<typename T>
class TriodeZDF {
    static_assert(std::is_floating_point_v<T>, "TriodeZDF<T>: T must be float/double");

public:
    TriodeZDF() = default;

    // ---------- lifecycle ----------
    void prepare(T sampleRate) {
        mFs = std::max(sampleRate, T(1));
        mT  = T(1) / mFs;
        reset();
    }

    void reset() {
        // Start at a reasonable bias point: cathode ~0, plate ~B+
        mVk_z = T(0);
        mVk   = T(0);
        mVp   = mBplus;
        mLastIn = T(0);
        mLastOut = T(0);
    }

    // ---------- user parameters (audio domain) ----------
    void setDrive(T d)      { mDrive = std::max(d, T(0)); }
    void setBias(T b)       { mBias  = b; }           // in "input units"
    void setOutputTrim(T t) { mTrim  = t; }
    void setCalibration(T refVolts) { mRefV = std::max(refVolts, T(1e-6)); } // input units -> volts

    // ---------- circuit parameters ----------
    void setBplus(T v) { mBplus = std::max(v, T(1)); }

    void setPlateResistor(T ohms)   { mRp = std::max(ohms, T(1)); }
    void setCathodeResistor(T ohms) { mRk = std::max(ohms, T(1)); }
    void setCathodeCap(T farads)    { mCk = std::max(farads, T(0)); }

    // ---------- triode model parameters ----------
    // mu: amplification factor (controls Vpk influence)
    void setMu(T mu) { mMu = std::max(mu, T(1)); }

    // G: plate current scale (A / V^(3/2)) roughly
    void setG(T G) { mG = std::max(G, T(0)); }

    // Va: “plate softening” scale (V). Larger => less plate dependence.
    void setVa(T Va) { mVa = std::max(Va, T(1)); }

    // Vt: smoothing/knee volts for softplus (controls cutoff sharpness)
    void setVt(T Vt) { mVt = std::max(Vt, T(1e-4)); }

    // grid conduction strength + knee
    void setGridG(T Gg) { mGg = std::max(Gg, T(0)); }
    void setGridVt(T Vtg) { mVtg = std::max(Vtg, T(1e-4)); }

    // solver
    void setNewtonIters(int iters) { mIters = std::clamp(iters, 1, 16); }
    void setNewtonEps(T eps) { mEps = std::max(eps, T(1e-12)); }

    // ---------- processing ----------
    T processSample(T x) {
        // 1) input -> grid volts
        const T vg = (x * mDrive + mBias) * mRefV;

        // 2) solve implicit stage for Vk, Vp
        solveNewton_(vg);

        // 3) choose an output mapping
        // A common “triode stage out” is the plate node, AC-coupled later.
        // Here we center around mid-supply to produce audio-ish bipolar output.
        const T vout = (mVp - T(0.5) * mBplus);

        T y = (vout / mRefV) * mTrim;

        mLastIn = x;
        mLastOut = y;
        return y;
    }

    // debug taps
    T cathodeV() const { return mVk; }
    T plateV() const { return mVp; }
    T lastIn() const { return mLastIn; }
    T lastOut() const { return mLastOut; }

private:
    // ---------- numerics helpers ----------
    static T clamp(T v, T lo, T hi) { return std::min(hi, std::max(lo, v)); }

    // stable softplus in volts: softplus(x) = ln(1+exp(x/Vt))*Vt
    static T softplusV_(T x, T Vt) {
        const T s = x / Vt;
        // avoid overflow/underflow
        if (s > T(40))  return x;                 // ln(1+exp(s)) ~ s
        if (s < T(-40)) return T(0);              // exp(s) ~ 0
        return Vt * std::log1p(std::exp(s));
    }

    // Smooth 3/2 power for x>=0
    static T pow32_(T x) {
        x = std::max(x, T(0));
        return x * std::sqrt(x);
    }

    // ---------- device currents ----------
    // Minimal smooth “triode-ish” model:
    // Ip = G * [ softplus( Vgk + Vpk/mu ) ]^(3/2) * (1 + Vpk/Va)  with Vpk clamped >=0
    //
    // Notes:
    // - softplus keeps it smooth at cutoff (no kinks).
    // - (1 + Vpk/Va) adds mild plate dependence.
    T plateCurrent_(T Vgk, T Vpk) const {
        Vpk = std::max(Vpk, T(0)); // don’t allow negative plate-cathode voltage to create weirdness
        const T drive = softplusV_(Vgk + (Vpk / mMu), mVt);
        const T child = pow32_(drive);
        const T plate = (T(1) + Vpk / mVa);
        return mG * child * plate;
    }

    // Grid conduction when Vgk > 0, smoothed:
    // Ig = Gg * softplus(Vgk)^(3/2)
    T gridCurrent_(T Vgk) const {
        if (mGg <= T(0)) return T(0);
        const T g = softplusV_(Vgk, mVtg);
        return mGg * pow32_(g);
    }

    // ---------- residuals ----------
    // f1(Vk, Vp) = Vk/Rk + Ck*(Vk - Vk_z)/T - (Ip + Ig)
    // f2(Vk, Vp) = (B+ - Vp)/Rp - Ip
    void residuals_(T vg, T Vk, T Vp, T& f1, T& f2) const {
        const T Vgk = vg - Vk;
        const T Vpk = Vp - Vk;

        const T Ip = plateCurrent_(Vgk, Vpk);
        const T Ig = gridCurrent_(Vgk);

        const T gk = (mRk > T(0)) ? (Vk / mRk) : T(0);
        const T ck = (mCk > T(0)) ? (mCk * (Vk - mVk_z) / mT) : T(0);

        f1 = gk + ck - (Ip + Ig);
        f2 = (mBplus - Vp) / mRp - Ip;
    }

    // Finite-difference Jacobian (simple + robust + no symbolic mistakes)
    void jacobianFD_(T vg, T Vk, T Vp,
                     T& a11, T& a12, T& a21, T& a22) const
    {
        const T hV = T(1e-6); // volts
        T f1, f2;
        residuals_(vg, Vk, Vp, f1, f2);

        // d/dVk
        {
            T f1p, f2p;
            residuals_(vg, Vk + hV, Vp, f1p, f2p);
            a11 = (f1p - f1) / hV;
            a21 = (f2p - f2) / hV;
        }

        // d/dVp
        {
            T f1p, f2p;
            residuals_(vg, Vk, Vp + hV, f1p, f2p);
            a12 = (f1p - f1) / hV;
            a22 = (f2p - f2) / hV;
        }
    }

    // Solve 2x2 linear system:
    // [a11 a12][dVk] = -[f1]
    // [a21 a22][dVp]    [f2]
    static void solve2x2_(T a11, T a12, T a21, T a22, T f1, T f2, T& dVk, T& dVp, T eps) {
        const T det = a11 * a22 - a12 * a21;
        if (std::abs(det) < eps) {
            // fallback: tiny step (prevents NaNs)
            dVk = T(0);
            dVp = T(0);
            return;
        }
        const T invDet = T(1) / det;
        dVk = (-f1 * a22 - (-f2) * a12) * invDet; // = (-f1*a22 + f2*a12)/det
        dVp = ( a21 * (-f1) + a11 * (-f2)) * invDet; // = (-a21*f1 - a11*f2)/det
    }

    void solveNewton_(T vg) {
        // initial guess: last state (good for ZDF)
        T Vk = mVk;
        T Vp = mVp;

        // hard clamps for sanity
        const T VkMin = T(-50);
        const T VkMax = mBplus;
        const T VpMin = T(0);
        const T VpMax = mBplus;

        for (int i = 0; i < mIters; ++i) {
            T f1, f2;
            residuals_(vg, Vk, Vp, f1, f2);

            // stop if small
            if (std::abs(f1) + std::abs(f2) < T(1e-9)) break;

            T a11, a12, a21, a22;
            jacobianFD_(vg, Vk, Vp, a11, a12, a21, a22);

            T dVk, dVp;
            solve2x2_(a11, a12, a21, a22, f1, f2, dVk, dVp, mEps);

            // damping helps a lot with hard drive + grid conduction
            T lambda = T(1);
            for (int ls = 0; ls < 6; ++ls) {
                const T VkTry = clamp(Vk + lambda * dVk, VkMin, VkMax);
                const T VpTry = clamp(Vp + lambda * dVp, VpMin, VpMax);

                T f1t, f2t;
                residuals_(vg, VkTry, VpTry, f1t, f2t);

                if (std::abs(f1t) + std::abs(f2t) <= std::abs(f1) + std::abs(f2)) {
                    Vk = VkTry;
                    Vp = VpTry;
                    break;
                }
                lambda *= T(0.5);
            }
        }

        // commit
        mVk   = Vk;
        mVp   = Vp;
        mVk_z = Vk; // BE integrator state for cathode capacitor
    }

private:
    // sample rate
    T mFs = T(48000);
    T mT  = T(1) / T(48000);

    // audio params
    T mDrive = T(1);
    T mBias  = T(0);
    T mTrim  = T(1);
    T mRefV  = T(1); // volts per 1.0 input unit

    // circuit
    T mBplus = T(250);
    T mRp    = T(100000);  // 100k plate load
    T mRk    = T(1500);    // 1.5k cathode resistor
    T mCk    = T(0);       // cathode bypass cap (0 => none)

    // triode model
    T mMu = T(80);
    T mG  = T(2e-6);   // scale: tune with drive/refV
    T mVa = T(200);    // plate dependence scale
    T mVt = T(0.2);    // softplus knee volts

    // grid conduction
    T mGg  = T(2e-6);
    T mVtg = T(0.1);

    // solver
    int mIters = 6;
    T   mEps   = T(1e-12);

    // states
    T mVk_z = T(0);
    T mVk   = T(0);
    T mVp   = T(0);

    // debug
    T mLastIn  = T(0);
    T mLastOut = T(0);
};

} // namespace gdsp
