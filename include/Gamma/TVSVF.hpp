#pragma once
#include <cmath>
#include <algorithm>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

enum TVSVFType : unsigned {
    TVSVFLowpass = 0,
    TVSVFBandpass,
    TVSVFHighpass,
    TVSVFNotch,
    TVSVFAllpass,
    TVSVFPeak
};

template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class TVSVF : public Td {
public:
    struct Out {
        Tv hp, bp, lp, notch, ap, peak;
    };

    TVSVF(Tp freqHz = Tp(1000), Tp Q = Tp(0.707), TVSVFType type = TVSVFLowpass)
    : mType(type)
    {
        set(freqHz, Q);
        reset();
        updateMix();
    }

    // -------- params --------
    void type(TVSVFType t){
        mType = t;
        updateMix();
    }

    TVSVFType type() const { return mType; }

    void freq(Tp hz){
        mFreq = hz;
        compute();
    }

    Tp freq() const { return mFreq; }

    void res(Tp Q){
        mQ = Q;
        compute();
        updateMix(); // allpass depends on k
    }

    Tp res() const { return mQ; }

    void set(Tp hz, Tp Q){
        mFreq = hz;
        mQ = Q;
        compute();
        updateMix();
    }

    // Canonical coeff injection (the “TVSVF contract”)
    // g,k are canonical SVF coeffs; hp/bp/lp are mix weights.    
    void setCoeffs(Tp newG, Tp newK){
        mG = newG;
        mK = newK;
        recalcDerived();
        updateMix();
    }

    void mix(Tp hpW, Tp bpW, Tp lpW){
        mHPW = hpW;
        mBPW = bpW;
        mLPW = lpW;
    }

    // -------- state --------
    // only showing changed parts
    void reset(Tv s1 = Tv(0), Tv s2 = Tv(0)){
        mS1 = s1;
        mS2 = s2;
    }


    // -------- processing --------

    /// Process and return selected output (based on type())
    Tv operator()(Tv x){
        const auto o = processAll(x);
        switch(mType){
            case TVSVFLowpass:  return o.lp;
            case TVSVFBandpass: return o.bp;
            case TVSVFHighpass: return o.hp;
            case TVSVFNotch:    return o.notch;
            case TVSVFAllpass:  return o.ap;
            case TVSVFPeak:     return o.peak;
            default:            return o.lp;
        }
    }

    /// Process and return full output bundle
    Out processAll(Tv x){
        // ZDF / TPT SVF core (Zavalishin form)
        const Tv v3 = x - mS2;
        const Tv v1 = Tv(mA1) * mS1 + Tv(mA2) * v3;
        const Tv v2 = mS2 + Tv(mA2) * mS1 + Tv(mA3) * v3;

        // trapezoidal integrators (state update)
        mS1 = Tv(2) * v1 - mS1;
        mS2 = Tv(2) * v2 - mS2;

        const Tv hp = x - Tv(mK) * v1 - v2;
        const Tv bp = v1;
        const Tv lp = v2;

        const Tv notch = hp + lp;
        const Tv ap    = notch - Tv(mK) * bp;  // hp + lp - k*bp
        const Tv peak  = lp - hp;

        return {hp, bp, lp, notch, ap, peak};
    }

    /// Process and return weighted mix of hp/bp/lp (your “mHP/mBP/mLP” style)
    Tv processMix(Tv x){
        const auto o = processAll(x);
        return Tv(mHPW) * o.hp + Tv(mBPW) * o.bp + Tv(mLPW) * o.lp;
    }

    void onDomainChange(double){
        compute();
        updateMix();
    }

private:
    // params
    TVSVFType mType = TVSVFLowpass;
    Tp mFreq = Tp(1000);
    Tp mQ    = Tp(0.707);

    // canonical coeffs
    Tp mG = Tp(0);
    Tp mK = Tp(1);     // 1/Q

    // derived coeffs
    Tp mA1 = Tp(1);
    Tp mA2 = Tp(0);
    Tp mA3 = Tp(0);

    // state (ZDF integrator equiv currents)
    Tv mS1 = Tv(0);
    Tv mS2 = Tv(0);

    // mix weights (hp/bp/lp)
    Tp mHPW = Tp(0), mBPW = Tp(0), mLPW = Tp(1);

    void compute(){
        const Tp fs = Tp(Td::spu());
        if(fs <= Tp(0)) return;

        const Tp nyq = fs * Tp(0.5);
        const Tp f   = std::clamp(mFreq, Tp(0), nyq * Tp(0.999));
        const Tp Q   = std::max(mQ, Tp(1e-6));

        mFreq = f;
        mQ    = Q;

        mG = std::tan(Tp(M_PI) * (mFreq / fs));
        mK = Tp(1) / mQ;

        recalcDerived();
    }


    void recalcDerived(){
        const Tp den = Tp(1) + mG * (mG + mK);
        mA1 = Tp(1) / std::max(den, Tp(1e-12));
        mA2 = mG * mA1;
        mA3 = mG * mA2;
    }

    void updateMix(){
        switch(mType){
            case TVSVFLowpass:  mHPW = Tp(0);  mBPW = Tp(0);   mLPW = Tp(1); break;
            case TVSVFBandpass: mHPW = Tp(0);  mBPW = Tp(1);   mLPW = Tp(0); break;
            case TVSVFHighpass: mHPW = Tp(1);  mBPW = Tp(0);   mLPW = Tp(0); break;
            case TVSVFNotch:    mHPW = Tp(1);  mBPW = Tp(0);   mLPW = Tp(1); break;
            case TVSVFAllpass:  mHPW = Tp(1);  mBPW = -mK;     mLPW = Tp(1); break;
            case TVSVFPeak:     mHPW = Tp(-1); mBPW = Tp(0);   mLPW = Tp(1); break;
            default:            mHPW = Tp(0);  mBPW = Tp(0);   mLPW = Tp(1); break;
        }
    }
};

} // namespace gam
