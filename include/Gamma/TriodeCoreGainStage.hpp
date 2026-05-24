#pragma once
#include <algorithm>
#include <cmath>
#include <type_traits>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

// If you already have your Triode<T> from earlier, use that instead.
template<typename T>
struct TriodeCore {
    static_assert(std::is_floating_point_v<T>, "TriodeCore<T>: T must be float/double");

    T mu  = T(100);
    T kg1 = T(1060);
    T kp  = T(600);
    T ex  = T(1.4);
    T vct = T(0.0);

    T driveSat     = T(0.7);
    T cutoffSmooth = T(0.02);
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
// TriodeGainStage: explicit plate RC stage (no Newton), Gamma-idiomatic
//
// Node equation implemented (forward Euler on a leaky integrator):
//   Cp dVp/dt = (B+ - Vp)/Rp - Ip
//
// This captures:
// - plate load compression
// - "sag" like recovery from current draw (via Rp*Cp)
// - realistic interaction with Ip through Vpk
//
// Output is AC-coupled-ish by centering around B+/2 (or you can add an HPF later).
// -----------------------------------------------------------------------------
template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class TriodeGainStage : public Td {
public:
    using T = Tv;

    TriodeGainStage(){
        reset();
    }

    // ----------------------------
    // Gamma lifecycle
    // ----------------------------
    void reset(){
        mVp = mBplus;  // start at supply
        mLastIn = T(0);
        mLastOut = T(0);
    }

    void onDomainChange(double){
        mFs = std::max(T(Td::spu()), T(1));
        mTs = T(1) / mFs;
        updatePlateCoeff_();
    }

    // ----------------------------
    // Parameters (audio/calibration)
    // ----------------------------
    void drive(T d){ mDrive = std::max(d, T(0)); }
    void bias(T b){ mBias = b; }           // input units
    void trim(T t){ mTrim = t; }
    void calibration(T refVolts){ mRefV = std::max(refVolts, T(1e-6)); }

    // ----------------------------
    // Stage topology
    // ----------------------------
    void bplus(T v){ mBplus = std::max(v, T(1)); }
    void rp(T ohms){ mRp = std::max(ohms, T(1)); updatePlateCoeff_(); }
    void cp(T farads){ mCp = std::max(farads, T(0)); updatePlateCoeff_(); }

    // optional output centering / scaling
    void outputCenter(T v){ mOutCenter = v; } // default B+/2

    // access triode device model
    TriodeCore<T>& triode(){ return mTriode; }
    const TriodeCore<T>& triode() const { return mTriode; }

    // ----------------------------
    // Processing
    // ----------------------------
    inline T operator()(T x){
        // grid voltage
        const T Vg = (x * mDrive + mBias) * mRefV;

        // plate-cathode voltage (cathode assumed at 0 in this simplified stage)
        // You can extend later with a cathode network if desired.
        const T Vpk = std::max(mVp, T(0.05));

        // plate current
        const T Ip = mTriode(Vg /*vgk*/, Vpk);

        // plate node update (explicit RC from KCL):
        //
        // Cp dVp/dt = (B+ - Vp)/Rp - Ip
        //
        // Forward Euler:
        // Vp += (T/Cp) * [ (B+ - Vp)/Rp - Ip ]
        //
        // If Cp == 0, we fall back to instantaneous resistive plate: Vp = B+ - Ip*Rp.
        if (mCp > T(0)) {
            const T iRp = (mBplus - mVp) * mInvRp;
            const T dVp = (mTs * mInvCp) * (iRp - Ip);
            mVp += dVp;

            // hard safety clamp
            mVp = clamp_(mVp, T(0), mBplus);
        } else {
            mVp = clamp_(mBplus - Ip * mRp, T(0), mBplus);
        }

        // output: inverted plate swing, centered
        const T center = (mOutCenter >= T(0)) ? mOutCenter : (T(0.5) * mBplus);
        const T vout = -(mVp - center);

        const T y = (vout / mRefV) * mTrim;

        mLastIn = x;
        mLastOut = y;
        return y;
    }

    // debug taps
    T plateV() const { return mVp; }
    T lastIn() const { return mLastIn; }
    T lastOut() const { return mLastOut; }

private:
    static inline T clamp_(T v, T lo, T hi){ return std::min(hi, std::max(lo, v)); }

    void updatePlateCoeff_(){
        mInvRp = (mRp > T(0)) ? (T(1) / mRp) : T(0);
        mInvCp = (mCp > T(0)) ? (T(1) / mCp) : T(0);
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

    // topology
    T mBplus { T(250) };
    T mRp    { T(100000) };
    T mCp    { T(10e-12) };   // small parasitic by default (10 pF)
    T mInvRp { T(1) / T(100000) };
    T mInvCp { T(0) };

    // output centering (set <0 to mean "use B+/2")
    T mOutCenter { T(-1) };

    // device
    TriodeCore<T> mTriode;

    // state
    T mVp { T(250) };

    // debug
    T mLastIn  { T(0) };
    T mLastOut { T(0) };
};

} // namespace gam
