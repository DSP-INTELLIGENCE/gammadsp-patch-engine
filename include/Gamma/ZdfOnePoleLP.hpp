#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include <cmath>
#include <algorithm>

namespace gam {

template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ZdfOnePoleLP : public Td {
public:
    ZdfOnePoleLP(){
        setFreq(Tp(1000));
    }

    /// Set cutoff frequency (Hz in default domain)
    void setFreq(Tp freqHz){
        mFreq = std::max<Tp>(Tp(0), freqHz);
        update();
    }

    Tp freq() const { return mFreq; }

    void reset(Tv v = Tv(0)){
        mState = v;
    }

    /// ZDF one-pole low-pass
    Tv operator()(Tv x){
        // v = (x + g*s) / (1 + g)
        const Tv v = (x + Tv(mG) * mState) * Tv(mA);

        // integrator update (TPT form)
        mState = v + (v - mState);

        return v;
    }

    /// React to domain / sample-rate changes
    void onDomainChange(double){
        update();
    }

private:
    void update(){
        const Tp fs = Tp(Td::spu());
        if(fs <= Tp(0)){
            mG = Tp(0);
            mA = Tp(1);
            return;
        }

        // clamp below Nyquist for tan() safety
        const Tp nyq = fs * Tp(0.5);
        const Tp f   = std::clamp(mFreq, Tp(0), nyq * Tp(0.999));

        // TPT coefficient
        mG = std::tan(Tp(M_PI) * (f / fs));

        // 1 / (1 + g)
        mA = Tp(1) / (Tp(1) + mG);
    }

private:
    Tp mFreq  = Tp(1000);
    Tp mG     = Tp(0);   // tan(pi*f/fs)
    Tp mA     = Tp(1);   // 1 / (1 + g)
    Tv mState = Tv(0);   // integrator state
};

} // namespace gam
