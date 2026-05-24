#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include <cmath>

namespace gam {

// assumes ZdfSVF + ZdfParametricBand already exist

template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ZdfEQ3 : public Td {
public:
    using Band = ZdfParametricBand<Tv,Tp,Td>;

    ZdfEQ3(){
        // sensible defaults
        setLow (Tp(120),  Tp(0.707), Tp(0));
        setMid (Tp(1200), Tp(1.0),   Tp(0));
        setHigh(Tp(8000), Tp(0.707), Tp(0));
    }

    /// Reset all band states
    void reset(Tv v = Tv(0)){
        mLow.reset(v);
        mMid.reset(v);
        mHigh.reset(v);
    }

    // ---- Low shelf ----
    void setLow(Tp freqHz, Tp Q, Tp gainDb){
        mLow.setMode(Band::Mode::LowShelf);
        mLow.setFreq(freqHz);
        mLow.setQ(Q);
        mLow.setGainDb(gainDb);
    }

    void setLowFreq(Tp f){ mLow.setFreq(f); }
    void setLowQ(Tp q){ mLow.setQ(q); }
    void setLowGainDb(Tp db){ mLow.setGainDb(db); }

    // ---- Mid bell ----
    void setMid(Tp freqHz, Tp Q, Tp gainDb){
        mMid.setMode(Band::Mode::Bell);
        mMid.setFreq(freqHz);
        mMid.setQ(Q);
        mMid.setGainDb(gainDb);
    }

    void setMidFreq(Tp f){ mMid.setFreq(f); }
    void setMidQ(Tp q){ mMid.setQ(q); }
    void setMidGainDb(Tp db){ mMid.setGainDb(db); }

    // ---- High shelf ----
    void setHigh(Tp freqHz, Tp Q, Tp gainDb){
        mHigh.setMode(Band::Mode::HighShelf);
        mHigh.setFreq(freqHz);
        mHigh.setQ(Q);
        mHigh.setGainDb(gainDb);
    }

    void setHighFreq(Tp f){ mHigh.setFreq(f); }
    void setHighQ(Tp q){ mHigh.setQ(q); }
    void setHighGainDb(Tp db){ mHigh.setGainDb(db); }

    /// Process one sample
    Tv operator()(Tv x){
        x = mLow(x);
        x = mMid(x);
        x = mHigh(x);
        return x;
    }

private:
    Band mLow;
    Band mMid;
    Band mHigh;
};

} // namespace gam
