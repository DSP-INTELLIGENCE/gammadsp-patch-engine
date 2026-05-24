#pragma once
#include "Gamma/Filter.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

template<
    unsigned N = 6,
    class Tv = gam::real,
    class Tp = gam::real,
    class Td = GAM_DEFAULT_DOMAIN
>
class AcousticLens : public Td {
public:
    AcousticLens(){
        for(unsigned i = 0; i < N; ++i){
            mWarp[i].freq(700 + Tp(i) * 300);
        }
    }

    /// 0..1 — spectral focal point
    void focus(Tp v){
        mFocus = scl::clip(v, Tp(0), Tp(1));
    }

    /// 0..1 — modulation depth
    void depth(Tp v){
        mDepth = scl::clip(v, Tp(0), Tp(1));
    }

    void reset(){
        for(auto& a : mWarp) a.reset();
        mShift.reset();
        mFM.reset();
    }

    /// Process one sample
    Tv operator()(Tv x){
        Tv y = x;

        // ---- Phase warp (lens curvature) ----
        Tp f = Tp(300) + mFocus * Tp(4000);
        for(auto& a : mWarp){
            a.freq(f);
            y = a(y);
        }

        // ---- Spectral refraction ----
        Tp refraction = (mFocus - Tp(0.5)) * Tp(40);
        mShift.shift(refraction);
        y = mShift(y);

        // ---- Micro dispersion ----
        mFM.depth(mDepth * Tp(0.02));
        y = mFM(y);

        return y;
    }

protected:
    // Phase warp
    AllPass1<Tv,Tp,Td> mWarp[N];

    // Frequency refraction
    FrequencyShifter<Tv,Tp,Td> mShift;

    // Micro FM diffusion
    ModFM<Tv,Tp,Td> mFM;

    Tp mFocus = Tp(0.5);
    Tp mDepth = Tp(0.3);
};

} // namespace gam
