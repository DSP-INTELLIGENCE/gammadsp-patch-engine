#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/Filter.h"
#include "Gamma/scl.h"

namespace gam {

template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class FrequencyShifter : public Td {
public:
    FrequencyShifter(Tp shiftHz = Tp(100))
    : mShift(shiftHz)
    {
        imSmooth(0.001f);
        amSmooth(0.001f);
        reset();
    }

    /// Base frequency shift in Hz
    void shift(Tp hz){ mShift = hz; }

    /// Input gain modulation (linear)
    void imSmooth(Tp lagSec){ mIM.lag(lagSec); }

    /// Output gain modulation (linear)
    void amSmooth(Tp lagSec){ mAM.lag(lagSec); }

    void reset(){
        mPhase = Tp(0);
        mHilbert.reset();
        mDC.reset();
        mIM.reset(Tp(1));
        mAM.reset(Tp(1));
    }

    /// Process one sample
    Tv operator()(Tv x){
        // ---- input conditioning ----
        x = mIM(x);
        x = mDC(x);

        // ---- oscillator ----
        Tp sr = Tp(Td::sampleRate());
        Tp dphi = Tp(2 * M_PI) * mShift / sr;
        mPhase += dphi;

        // wrap phase
        if(mPhase >  Tp(M_PI)) mPhase -= Tp(2 * M_PI);
        if(mPhase < -Tp(M_PI)) mPhase += Tp(2 * M_PI);

        // ---- analytic signal ----
        auto z = mHilbert(x); // Complex<Tv>
        Tp c = scl::cos(mPhase);
        Tp s = scl::sin(mPhase);

        // real part of complex multiply
        Tv y = z.real() * c - z.imag() * s;

        return mAM(y);
    }

protected:
    Tp mShift;

    Tp mPhase = Tp(0);

    Hilbert<Tv,Tp> mHilbert;
    BlockDC<Tv,Tp,Td> mDC;

    OnePole<Tv,Tp,Td> mIM; // input smoothing
    OnePole<Tv,Tp,Td> mAM; // output smoothing
};

} // namespace gam
