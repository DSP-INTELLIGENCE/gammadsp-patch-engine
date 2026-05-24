// AttackGenerator.hpp
#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class AttackGenerator : public Td {
public:
    enum class Shape { Exponential, Linear, SCurve };

    AttackGenerator(){
        time(Tv(0.01));
        shape(Shape::Exponential);
    }

    // ---- parameters ----
    void time(Tv seconds){
        mTime = std::max(seconds, Tv(1e-6));
        updateCoeff();
    }

    void shape(Shape s){ mShape = s; }

    /// 0 = fixed, 1 = strongly program dependent
    void program(Tv amount){
        mProgram = std::clamp(amount, Tv(0), Tv(1));
    }

    // ---- state ----
    void reset(Tv v = Tv(0)){
        mValue = v;
    }

    // ---- processing ----
    inline Tv operator()(Tv input){
        const Tv delta = input - mValue;

        // attack-only
        if(delta <= Tv(0))
            return mValue;

        // program-dependent speed-up
        Tv coeff = mCoeff;
        if(mProgram > Tv(0)){
            Tv scale = Tv(1) + mProgram * std::min(std::abs(delta), Tv(10));
            coeff = std::pow(coeff, scale);
        }

        switch(mShape){
        case Shape::Linear:
            mValue += (Tv(1) - coeff) * delta;
            break;

        case Shape::SCurve:{
            Tv t = Tv(1) - coeff;
            // smoothstep
            Tv s = t * t * (Tv(3) - Tv(2) * t);
            mValue += s * delta;
            break;
        }

        case Shape::Exponential:
        default:
            mValue = (Tv(1) - coeff) * input + coeff * mValue;
            break;
        }

        return mValue;
    }

    Tv value() const { return mValue; }

    // ---- Gamma hook ----
    void onDomainChange(double){
        updateCoeff();
    }

private:
    void updateCoeff(){
        const Tv fs = Tv(Td::spu());
        if(fs <= Tv(0)) return;

        // classic one-pole envelope coefficient
        mCoeff = std::exp(Tv(-1) / (mTime * fs));
    }

private:
    Tv mTime   { Tv(0.01) };
    Tv mCoeff  { Tv(0) };
    Tv mValue  { Tv(0) };
    Tv mProgram{ Tv(0) };
    Shape mShape { Shape::Exponential };
};

} // namespace gam
