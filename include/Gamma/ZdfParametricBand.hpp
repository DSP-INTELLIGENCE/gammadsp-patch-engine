#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include <cmath>
#include <algorithm>

namespace gam {

// --- ZDF SVF core (your existing class) ---
// Assumes you already have ZdfSVF<Tv,Tp,Td> with outputs: hp, bp, lp, notch, ap

template<class Tp>
static inline Tp dbToLin(Tp db){
    return std::pow(Tp(10), db / Tp(20));
}

template<class Tp>
static inline Tp clampQ(Tp q){
    return std::max<Tp>(Tp(1e-6), q);
}

template<class Tp>
static inline Tp clampFreq(Tp f, Tp fs){
    // keep below Nyquist to avoid tan() blow-up in SVF update()
    Tp nyq = fs * Tp(0.5);
    return std::clamp(f, Tp(0), nyq * Tp(0.999));
}

template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ZdfParametricBand : public Td {
public:
    enum class Mode : unsigned {
        Bell,
        LowShelf,
        HighShelf,
        Tilt
    };

    ZdfParametricBand(){
        setMode(Mode::Bell);
        setFreq(Tp(1000));
        setQ(Tp(0.707));
        setGainDb(Tp(0));
    }

    void reset(Tv v = Tv(0)){ mSVF.reset(v); }

    void setMode(Mode m){ mMode = m; }

    void setFreq(Tp freqHz){
        const Tp fs = Tp(Td::spu());
        mFreq = (fs > Tp(0)) ? clampFreq(freqHz, fs) : std::max<Tp>(Tp(0), freqHz);
        mSVF.setFreq(mFreq);
    }

    void setQ(Tp q){
        mQ = clampQ(q);
        mSVF.setQ(mQ);
    }

    /// Gain in dB (boost/cut). For tilt, this is also in dB.
    void setGainDb(Tp db){
        mGainDb = db;
        mGainLin = dbToLin(db);     // e.g. +6dB -> ~1.995
        // For “x + k*filterOutput” forms, we want k centered at 0:
        mK = mGainLin - Tp(1);      // 0 at 0dB, positive boost, negative cut
    }

    Mode mode() const { return mMode; }
    Tp   freq() const { return mFreq; }
    Tp   Q()    const { return mQ; }
    Tp   gainDb() const { return mGainDb; }

    /// Process one sample
    Tv operator()(Tv x){
        const auto o = mSVF(x);

        switch(mMode){
            case Mode::Bell:
                // Classic state-variable “bell”: y = x + k * BP
                // k = (linearGain - 1)
                return x + Tv(mK) * o.bp;

            case Mode::LowShelf:
                // Low shelf: y = x + k * LP
                return x + Tv(mK) * o.lp;

            case Mode::HighShelf:
                // High shelf: y = x + k * HP
                return x + Tv(mK) * o.hp;

            case Mode::Tilt:
                // Tilt around freq: y = x + k * (HP - LP)
                // Positive -> brighter, negative -> darker
                return x + Tv(mK) * (o.hp - o.lp);
        }
        return x;
    }

    void onDomainChange(double){
        setFreq(mFreq);
        setQ(mQ);
    }

private:
    ZdfSVF<Tv,Tp,Td> mSVF;

    Mode mMode = Mode::Bell;

    Tp mFreq   = Tp(1000);
    Tp mQ      = Tp(0.707);
    Tp mGainDb = Tp(0);

    Tp mGainLin = Tp(1);   // 10^(dB/20)
    Tp mK       = Tp(0);   // mGainLin - 1
};

} // namespace gam
