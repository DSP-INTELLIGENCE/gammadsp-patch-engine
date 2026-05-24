#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include <cmath>
#include <algorithm>

namespace gam {

template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ZdfSVF : public Td {
public:
    struct Out {
        Tv hp, bp, lp, notch, ap;
    };

    ZdfSVF() { set(Tp(1000), Tp(0.707)); }

    /// freqHz: cutoff in Hz (or 1/seconds if using default time domain)
    /// Q: resonance / quality factor
    void set(Tp freqHz, Tp Q){
        setFreq(freqHz);
        setQ(Q);
        update();
    }

    void setFreq(Tp freqHz){
        mFreq = std::max<Tp>(Tp(0), freqHz);
        update();
    }

    void setQ(Tp Q){
        // avoid divide-by-zero; Q typically > 0
        mQ = std::max<Tp>(Tp(1e-6), Q);
        update();
    }

    Tp freq() const { return mFreq; }
    Tp Q()    const { return mQ;    }

    void reset(Tv v = Tv(0)){
        mIc1eq = v;
        mIc2eq = v;
    }

    /// Process one sample; returns all common outputs
    Out operator()(Tv x){
        // Zavalishin TPT SVF core
        // g = tan(pi*f/fs), k = 1/Q
        // a1 = 1 / (1 + g*(g + k))
        // a2 = g*a1
        // a3 = g*a2
        //
        // v3 = x - ic2eq
        // v1 = a1*ic1eq + a2*v3
        // v2 = ic2eq + a2*ic1eq + a3*v3
        // ic1eq = 2*v1 - ic1eq
        // ic2eq = 2*v2 - ic2eq
        //
        // hp = x - k*v1 - v2
        // bp = v1
        // lp = v2
        // notch = hp + lp
        // allpass (2nd-order) = hp + lp - k*bp

        const Tv v3 = x - mIc2eq;
        const Tv v1 = Tv(mA1) * mIc1eq + Tv(mA2) * v3;
        const Tv v2 = mIc2eq + Tv(mA2) * mIc1eq + Tv(mA3) * v3;

        mIc1eq = Tv(2) * v1 - mIc1eq;
        mIc2eq = Tv(2) * v2 - mIc2eq;

        const Tv hp = x - Tv(mK) * v1 - v2;
        const Tv bp = v1;
        const Tv lp = v2;
        const Tv notch = hp + lp;
        const Tv ap = notch - Tv(mK) * bp;

        return {hp, bp, lp, notch, ap};
    }

private:
    void update(){
        // Td::spu() = samples per unit (Hz if unit=seconds)
        const Tp fs = Tp(Td::spu());
        if(fs <= Tp(0)){
            // Domain not initialized; fall back to something sane
            mG = Tp(0);
            mK = Tp(1) / mQ;
            mA1 = Tp(1);
            mA2 = Tp(0);
            mA3 = Tp(0);
            return;
        }

        // clamp to < Nyquist to keep tan() well-behaved
        const Tp nyq = fs * Tp(0.5);
        const Tp f   = std::clamp(mFreq, Tp(0), nyq * Tp(0.999));

        mG = std::tan(Tp(M_PI) * (f / fs));
        mK = Tp(1) / mQ;

        const Tp denom = Tp(1) + mG * (mG + mK);
        mA1 = Tp(1) / denom;
        mA2 = mG * mA1;
        mA3 = mG * mA2;
    }

    // params
    Tp mFreq = Tp(1000);
    Tp mQ    = Tp(0.707);

    // derived
    Tp mG  = Tp(0);
    Tp mK  = Tp(1);   // 1/Q
    Tp mA1 = Tp(1);
    Tp mA2 = Tp(0);
    Tp mA3 = Tp(0);

    // integrator states (equivalent currents)
    Tv mIc1eq = Tv(0);
    Tv mIc2eq = Tv(0);
};

/// 2-pole highpass from the ZDF SVF
template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ZdfHighpass2 : public Td {
public:
    void set(Tp freqHz, Tp Q){ mSVF.set(freqHz, Q); }
    void setFreq(Tp f){ mSVF.setFreq(f); }
    void setQ(Tp q){ mSVF.setQ(q); }
    void reset(Tv v = Tv(0)){ mSVF.reset(v); }

    Tv operator()(Tv x){
        return mSVF(x).hp;
    }
private:
    ZdfSVF<Tv,Tp,Td> mSVF;
};

/// 2-pole bandpass from the ZDF SVF
template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ZdfBandpass2 : public Td {
public:
    void set(Tp freqHz, Tp Q){ mSVF.set(freqHz, Q); }
    void setFreq(Tp f){ mSVF.setFreq(f); }
    void setQ(Tp q){ mSVF.setQ(q); }
    void reset(Tv v = Tv(0)){ mSVF.reset(v); }

    Tv operator()(Tv x){
        return mSVF(x).bp;
    }
private:
    ZdfSVF<Tv,Tp,Td> mSVF;
};

/// 2-pole lowpass from the ZDF SVF
template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ZdfLowpass2 : public Td {
public:
    void set(Tp freqHz, Tp Q){ mSVF.set(freqHz, Q); }
    void setFreq(Tp f){ mSVF.setFreq(f); }
    void setQ(Tp q){ mSVF.setQ(q); }
    void reset(Tv v = Tv(0)){ mSVF.reset(v); }

    Tv operator()(Tv x){
        return mSVF(x).lp;   // FIXED
    }
private:
    ZdfSVF<Tv,Tp,Td> mSVF;
};

/// 2-pole allpass from the ZDF SVF
template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ZdfAllpass2 : public Td {
public:
    void set(Tp freqHz, Tp Q){ mSVF.set(freqHz, Q); }
    void setFreq(Tp f){ mSVF.setFreq(f); }
    void setQ(Tp q){ mSVF.setQ(q); }
    void reset(Tv v = Tv(0)){ mSVF.reset(v); }

    Tv operator()(Tv x){
        return mSVF(x).ap;
    }
private:
    ZdfSVF<Tv,Tp,Td> mSVF;
};

/// 2-pole notch from the ZDF SVF
template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ZdfNotch2 : public Td {
public:
    void set(Tp freqHz, Tp Q){ mSVF.set(freqHz, Q); }
    void setFreq(Tp f){ mSVF.setFreq(f); }
    void setQ(Tp q){ mSVF.setQ(q); }
    void reset(Tv v = Tv(0)){ mSVF.reset(v); }

    Tv operator()(Tv x){
        return mSVF(x).notch;  // FIXED
    }
private:
    ZdfSVF<Tv,Tp,Td> mSVF;
};

template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ZdfPeakEQ : public Td {
public:
    void set(Tp freqHz, Tp Q, Tp gain){
        mSVF.set(freqHz, Q);
        setGain(gain);
    }

    void setFreq(Tp f){ mSVF.setFreq(f); }
    void setQ(Tp q){ mSVF.setQ(q); }
    void reset(Tv v = Tv(0)){ mSVF.reset(v); }

    /// gain in linear amplitude (e.g. 0.5 = cut, 2.0 = boost)
    void setGain(Tp g){
        mGain = g - Tp(1);   // center at 0
    }

    Tv operator()(Tv x){
        auto o = mSVF(x);
        return x + mGain * o.bp;
    }

private:
    ZdfSVF<Tv,Tp,Td> mSVF;
    Tp mGain = Tp(0);
};

template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ZdfLowShelf : public Td {
public:
    void set(Tp freqHz, Tp Q, Tp gain){
        mSVF.set(freqHz, Q);
        setGain(gain);
    }

    void setFreq(Tp f){ mSVF.setFreq(f); }
    void setQ(Tp q){ mSVF.setQ(q); }
    void reset(Tv v = Tv(0)){ mSVF.reset(v); }

    void setGain(Tp g){
        mGain = g - Tp(1);
    }

    Tv operator()(Tv x){
        auto o = mSVF(x);
        return x + mGain * o.lp;
    }

private:
    ZdfSVF<Tv,Tp,Td> mSVF;
    Tp mGain = Tp(0);
};


template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ZdfHighShelf : public Td {
public:
    void set(Tp freqHz, Tp Q, Tp gain){
        mSVF.set(freqHz, Q);
        setGain(gain);
    }

    void setFreq(Tp f){ mSVF.setFreq(f); }
    void setQ(Tp q){ mSVF.setQ(q); }
    void reset(Tv v = Tv(0)){ mSVF.reset(v); }

    void setGain(Tp g){
        mGain = g - Tp(1);
    }

    Tv operator()(Tv x){
        auto o = mSVF(x);
        return x + mGain * o.hp;
    }

    void onDomainChange(double){
        setFreq(mFreq);
        setQ(mQ);
    }

private:
    ZdfSVF<Tv,Tp,Td> mSVF;
    Tp mGain = Tp(0);
};

template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ZdfTiltEQ : public Td {
public:
    void set(Tp freqHz, Tp Q, Tp gain){
        mSVF.set(freqHz, Q);
        setGain(gain);
    }

    void setFreq(Tp f){ mSVF.setFreq(f); }
    void setQ(Tp q){ mSVF.setQ(q); }
    void reset(Tv v = Tv(0)){ mSVF.reset(v); }

    void setGain(Tp g){
        mGain = g;
    }

    Tv operator()(Tv x){
        auto o = mSVF(x);
        return x + mGain * (o.hp - o.lp);
    }

private:
    ZdfSVF<Tv,Tp,Td> mSVF;
    Tp mGain = Tp(0);
};

} // namespace gam
