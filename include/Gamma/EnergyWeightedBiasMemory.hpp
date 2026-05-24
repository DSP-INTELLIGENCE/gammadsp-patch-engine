#pragma once
#include <cmath>
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class EnergyWeightedBiasMemory : public Td {
public:
    EnergyWeightedBiasMemory(){
        setEnergyAttack(T(0.010));   // 10 ms energy follower
        setEnergyRelease(T(0.080));  // 80 ms
        setBiasAttack(T(0.050));     // 50 ms bias pull
        setBiasRelease(T(0.500));    // 500 ms recovery

        setEnergyRef(T(1e-3));       // reference energy (~ -30 dBFS RMS^2)
        setBaseBias(T(0));
        setBiasDepth(T(0.2));

        setEnergyGate(T(0));         // 0 = off
        reset();
    }

    // ---------------------------
    // Controls
    // ---------------------------

    // Energy follower time constants (seconds)
    void setEnergyAttack(T sec){ mEAtk = scl::max(sec, T(1e-6)); updateECoeffs(); }
    void setEnergyRelease(T sec){ mERel = scl::max(sec, T(1e-6)); updateECoeffs(); }

    // Bias follower time constants (seconds)
    void setBiasAttack(T sec){ mBAtk = scl::max(sec, T(1e-6)); updateBCoeffs(); }
    void setBiasRelease(T sec){ mBRel = scl::max(sec, T(1e-6)); updateBCoeffs(); }

    // Reference energy for activity mapping:
    // a = E/(E + Eref)
    void setEnergyRef(T e){ mEref = scl::max(e, T(1e-12)); }

    // Bias mapping
    void setBaseBias(T b){ mBase = b; }
    void setBiasDepth(T d){ mDepth = d; }

    // Optional gate: do not move bias until energy exceeds this threshold (linear energy)
    void setEnergyGate(T eGate){ mGateE = scl::max(eGate, T(0)); }

    void reset(T bias = T(0)){
        mE = T(0);
        mBias = bias;
    }

    // ---------------------------
    // Processing
    // ---------------------------

    // Input: any signal whose "load" you want to track (recommended: driven input or wet output)
    // Output: current bias value
    inline T operator()(T x){
        // Energy input
        const T eIn = x * x;

        // Attack/release smoothing on energy (IIR)
        const T aE = (eIn > mE) ? mAlphaEAtk : mAlphaERel;
        mE += aE * (eIn - mE);

        // Optional energy gate
        if(mGateE > T(0) && mE < mGateE){
            // Only allow recovery toward base (slow)
            const T aB = mAlphaBRel;
            mBias += aB * ((mBase) - mBias);
            return mBias;
        }

        // Activity 0..1, saturating and scale-stable
        const T activity = mE / (mE + mEref);

        // Bias target from energy activity
        const T target = mBase + activity * mDepth;

        // Attack/release smoothing on bias (memory)
        const T aB = (target > mBias) ? mAlphaBAtk : mAlphaBRel;
        mBias += aB * (target - mBias);

        return mBias;
    }

    // Outputs for metering/visualization
    T bias() const { return mBias; }
    T energy() const { return mE; }

    void onDomainChange(double){
        updateECoeffs();
        updateBCoeffs();
    }

private:
    void updateECoeffs(){
        // alpha = 1 - exp(-1/(tau*Fs))
        const T fs = T(Td::spu());
        mAlphaEAtk = T(1) - scl::exp(T(-1) / (mEAtk * fs));
        mAlphaERel = T(1) - scl::exp(T(-1) / (mERel * fs));
    }

    void updateBCoeffs(){
        const T fs = T(Td::spu());
        mAlphaBAtk = T(1) - scl::exp(T(-1) / (mBAtk * fs));
        mAlphaBRel = T(1) - scl::exp(T(-1) / (mBRel * fs));
    }

private:
    // energy follower times
    T mEAtk = T(0.01), mERel = T(0.08);
    T mAlphaEAtk = T(0.01), mAlphaERel = T(0.001);

    // bias follower times
    T mBAtk = T(0.05), mBRel = T(0.5);
    T mAlphaBAtk = T(0.001), mAlphaBRel = T(0.0001);

    // mapping
    T mEref = T(1e-3);
    T mGateE = T(0);

    T mBase = T(0);
    T mDepth = T(0.2);

    // state
    T mE = T(0);
    T mBias = T(0);
};

} // namespace gam
