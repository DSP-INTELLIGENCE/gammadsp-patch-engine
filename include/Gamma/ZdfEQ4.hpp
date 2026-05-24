#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

// assumes ZdfSVF + ZdfParametricBand already exist

template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ZdfEQ4 : public Td {
public:
    using Band = ZdfParametricBand<Tv,Tp,Td>;

    ZdfEQ4(){
        // sensible musical defaults
        setLow     (Tp(120),   Tp(0.707), Tp(0));
        setLowMid  (Tp(400),   Tp(1.0),   Tp(0));
        setHighMid (Tp(2500),  Tp(1.0),   Tp(0));
        setHigh    (Tp(8000),  Tp(0.707), Tp(0));
    }

    /// Reset all band states
    void reset(Tv v = Tv(0)){
        mLow.reset(v);
        mLowMid.reset(v);
        mHighMid.reset(v);
        mHigh.reset(v);
    }

    /// Keep filters correct if the domain/sample-rate changes
    void onDomainChange(double){
        mLow.onDomainChange(1.0);
        mLowMid.onDomainChange(1.0);
        mHighMid.onDomainChange(1.0);
        mHigh.onDomainChange(1.0);
    }

    // ---- Low shelf ----
    void setLow(Tp freqHz, Tp Q, Tp gainDb){
        mLow.setMode(Band::Mode::LowShelf);
        mLow.setFreq(freqHz);
        mLow.setQ(Q);
        mLow.setGainDb(gainDb);
    }

    void setLowFreq(Tp f){ mLow.setFreq(f); }
    void setLowQ   (Tp q){ mLow.setQ(q); }
    void setLowGainDb(Tp db){ mLow.setGainDb(db); }

    // ---- Low-mid bell ----
    void setLowMid(Tp freqHz, Tp Q, Tp gainDb){
        mLowMid.setMode(Band::Mode::Bell);
        mLowMid.setFreq(freqHz);
        mLowMid.setQ(Q);
        mLowMid.setGainDb(gainDb);
    }

    void setLowMidFreq(Tp f){ mLowMid.setFreq(f); }
    void setLowMidQ   (Tp q){ mLowMid.setQ(q); }
    void setLowMidGainDb(Tp db){ mLowMid.setGainDb(db); }

    // ---- High-mid bell ----
    void setHighMid(Tp freqHz, Tp Q, Tp gainDb){
        mHighMid.setMode(Band::Mode::Bell);
        mHighMid.setFreq(freqHz);
        mHighMid.setQ(Q);
        mHighMid.setGainDb(gainDb);
    }

    void setHighMidFreq(Tp f){ mHighMid.setFreq(f); }
    void setHighMidQ   (Tp q){ mHighMid.setQ(q); }
    void setHighMidGainDb(Tp db){ mHighMid.setGainDb(db); }

    // ---- High shelf ----
    void setHigh(Tp freqHz, Tp Q, Tp gainDb){
        mHigh.setMode(Band::Mode::HighShelf);
        mHigh.setFreq(freqHz);
        mHigh.setQ(Q);
        mHigh.setGainDb(gainDb);
    }

    void setHighFreq(Tp f){ mHigh.setFreq(f); }
    void setHighQ   (Tp q){ mHigh.setQ(q); }
    void setHighGainDb(Tp db){ mHigh.setGainDb(db); }

    /// Optional: pass smoothing through to all bands
    void setSmoothingTimeMs(Tp ms){
        mLow.setSmoothingTimeMs(ms);
        mLowMid.setSmoothingTimeMs(ms);
        mHighMid.setSmoothingTimeMs(ms);
        mHigh.setSmoothingTimeMs(ms);
    }

    /// Process one sample
    Tv operator()(Tv x){
        x = mLow(x);
        x = mLowMid(x);
        x = mHighMid(x);
        x = mHigh(x);
        return x;
    }

private:
    Band mLow;
    Band mLowMid;
    Band mHighMid;
    Band mHigh;
};

} // namespace gam
