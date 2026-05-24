// ReleaseGenerator.hpp
#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ReleaseGenerator : public Td {
public:
    ReleaseGenerator(){
        time(Tv(0.1));
        program(Tv(0));
        hold(Tv(0));
    }

    // ---- parameters ----
    void time(Tv seconds){
        mBaseTime = std::max(seconds, Tv(1e-6));
        updateCoeff();
    }

    /// 0 = fixed, 1 = strongly program dependent
    void program(Tv amount){
        mProgram = std::clamp(amount, Tv(0), Tv(1));
    }

    /// Hold time in seconds before release begins
    void hold(Tv seconds){
        mHoldTime = std::max(seconds, Tv(0));
        updateHold();
    }

    // ---- state ----
    void reset(Tv initialDb = Tv(0)){
        mValueDb = initialDb;
        mHoldCounter = 0;
    }

    // ---- processing ----
    inline Tv operator()(Tv targetDb){
        // Still compressing → follow immediately
        if(targetDb < mValueDb){
            mValueDb = targetDb;
            mHoldCounter = mHoldSamples;
            return mValueDb;
        }

        // Hold phase
        if(mHoldCounter > 0){
            --mHoldCounter;
            return mValueDb;
        }

        // Releasing
        Tv coeff = mBaseCoeff;

        if(mProgram > Tv(0)){
            Tv delta = std::abs(targetDb - mValueDb);
            delta = std::min(delta, Tv(12)); // clamp for stability
            Tv scale = Tv(1) + mProgram * delta;
            coeff = std::pow(coeff, scale);
        }

        mValueDb = (Tv(1) - coeff) * targetDb + coeff * mValueDb;
        return mValueDb;
    }

    Tv value() const { return mValueDb; }

    // ---- Gamma hook ----
    void onDomainChange(double){
        updateCoeff();
        updateHold();
    }

private:
    void updateCoeff(){
        const Tv fs = Tv(Td::spu());
        if(fs <= Tv(0)) return;

        mBaseCoeff = std::exp(Tv(-1) / (mBaseTime * fs));
    }

    void updateHold(){
        const Tv fs = Tv(Td::spu());
        if(fs <= Tv(0)) return;

        mHoldSamples = int(mHoldTime * fs);
    }

private:
    // parameters
    Tv mBaseTime { Tv(0.1) };
    Tv mProgram  { Tv(0) };
    Tv mHoldTime { Tv(0) };

    // coefficients
    Tv mBaseCoeff { Tv(0) };

    // state
    Tv  mValueDb { Tv(0) };
    int mHoldSamples { 0 };
    int mHoldCounter { 0 };
};

} // namespace gam
