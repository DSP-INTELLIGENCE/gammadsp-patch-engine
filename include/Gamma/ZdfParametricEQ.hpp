#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include <algorithm>
#include <cmath>

namespace gam {

// ---------- small utilities ----------
template<class Tp>
static inline Tp dbToLin(Tp db){ return std::pow(Tp(10), db / Tp(20)); }

template<class Tp>
static inline Tp clampQ(Tp q){ return std::max<Tp>(Tp(1e-6), q); }

template<class Tp>
static inline Tp clampFreq(Tp f, Tp fs){
    Tp nyq = fs * Tp(0.5);
    return std::clamp(f, Tp(0), nyq * Tp(0.999));
}

template<class Tp>
static inline Tp onePoleSmooth(Tp y, Tp x, Tp a){
    // a in [0,1], smaller = smoother; y += a*(x-y)
    return y + a * (x - y);
}

// ---------- your SVF must exist ----------
template<class Tv, class Tp, class Td> class ZdfSVF;

// ---------- One parametric band ----------
template<class Tv = gam::real, class Tp = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ZdfParametricBand : public Td {
public:
    enum class Mode : unsigned { Bell, LowShelf, HighShelf, Tilt };

    struct Params {
        Mode mode = Mode::Bell;
        Tp   freqHz = Tp(1000);
        Tp   Q      = Tp(0.707);
        Tp   gainDb = Tp(0);
        bool bypass = false;
    };

    ZdfParametricBand(){
        setParams(Params{});
        setSmoothingTimeMs(Tp(10)); // default: 10 ms-ish
    }

    void reset(Tv v = Tv(0)){
        mSVF.reset(v);
        mFreq = mFreqT;
        mQ    = mQT;
        mK    = mKT;
        mBypassMix = mBypassT ? Tp(1) : Tp(0);
    }

    void setSmoothingTimeMs(Tp ms){
        // Convert to one-pole coefficient per sample:
        // a = 1 - exp(-1 / (tau * fs)), tau = ms/1000
        Tp fs = Tp(Td::spu());
        if(fs <= Tp(0) || ms <= Tp(0)){
            mA = Tp(1);
            return;
        }
        Tp tau = ms / Tp(1000);
        mA = Tp(1) - std::exp(Tp(-1) / (tau * fs));
        mA = std::clamp(mA, Tp(0), Tp(1));
    }

    void setParams(const Params& p){
        mModeT = p.mode;
        mFreqT = p.freqHz;
        mQT    = p.Q;
        mGainDbT = p.gainDb;
        mBypassT = p.bypass;

        // Update target k from gain dB
        Tp gainLin = dbToLin(mGainDbT);
        mKT = gainLin - Tp(1);

        // Push initial SVF targets (actual smoothing happens in process)
        // We'll clamp with actual fs in tickParams()
    }

    Params params() const {
        Params p;
        p.mode = mModeT;
        p.freqHz = mFreqT;
        p.Q = mQT;
        p.gainDb = mGainDbT;
        p.bypass = mBypassT;
        return p;
    }

    void onDomainChange(double) { mNeedUpdate = true; }

    /// Process one sample through this band
    Tv operator()(Tv x){
        tickParams();

        // Smooth bypass as mix: 0 = active, 1 = bypass
        // y = mix*x + (1-mix)*processed
        if(mBypassMix >= Tp(0.999))
            return x;

        auto o = mSVF(x);
        Tv processed = x;

        switch(mMode){
            case Mode::Bell:      processed = x + Tv(mK) * o.bp; break;
            case Mode::LowShelf:  processed = x + Tv(mK) * o.lp; break;
            case Mode::HighShelf: processed = x + Tv(mK) * o.hp; break;
            case Mode::Tilt:      processed = x + Tv(mK) * (o.hp - o.lp); break;
        }

        return Tv(mBypassMix) * x + Tv(Tp(1) - mBypassMix) * processed;
    }

private:
    void tickParams(){
        Tp fs = Tp(Td::spu());
        if(fs <= Tp(0)){
            // Domain not ready. Keep things inert.
            return;
        }

        // Clamp targets against current fs
        Tp freqT = clampFreq(mFreqT, fs);
        Tp qT    = clampQ(mQT);

        // Smooth
        mFreq = onePoleSmooth(mFreq, freqT, mA);
        mQ    = onePoleSmooth(mQ,    qT,   mA);
        mK    = onePoleSmooth(mK,    mKT,  mA);

        // Mode switches can be immediate (or smooth if you want)
        mMode = mModeT;

        // Smooth bypass toggle to avoid clicks
        Tp bypassMixT = mBypassT ? Tp(1) : Tp(0);
        mBypassMix = onePoleSmooth(mBypassMix, bypassMixT, mA);

        // Update SVF only when needed (cheap anyway, but this keeps intent clear)
        // For ZDF SVF, freq/Q changes require coefficient refresh.
        mSVF.setFreq(mFreq);
        mSVF.setQ(mQ);

        // If domain changed, this ensures re-clamp and SVF update
        if(mNeedUpdate){
            mNeedUpdate = false;
        }
    }

private:
    ZdfSVF<Tv,Tp,Td> mSVF;

    // targets
    Mode mModeT = Mode::Bell;
    Tp   mFreqT = Tp(1000);
    Tp   mQT    = Tp(0.707);
    Tp   mGainDbT = Tp(0);
    Tp   mKT    = Tp(0);
    bool mBypassT = false;

    // smoothed runtime
    Mode mMode = Mode::Bell;
    Tp   mFreq = Tp(1000);
    Tp   mQ    = Tp(0.707);
    Tp   mK    = Tp(0);
    Tp   mBypassMix = Tp(0); // 0=active, 1=bypass

    Tp   mA = Tp(1);         // smoothing coefficient
    bool mNeedUpdate = true;
};

// ---------- Full EQ strip ----------
template<
    unsigned NBands,
    class Tv = gam::real,
    class Tp = gam::real,
    class Td = GAM_DEFAULT_DOMAIN
>
class ZdfParametricEQ : public Td {
public:
    using Band = ZdfParametricBand<Tv,Tp,Td>;
    using Mode = typename Band::Mode;

    struct BandParams {
        Mode mode = Mode::Bell;
        Tp   freqHz = Tp(1000);
        Tp   Q      = Tp(0.707);
        Tp   gainDb = Tp(0);
        bool bypass = false;
    };

    ZdfParametricEQ(){
        setSmoothingTimeMs(Tp(10));
        setInputGainDb(Tp(0));
        setOutputGainDb(Tp(0));
    }

    void onDomainChange(double){
        for(unsigned i=0;i<NBands;++i) mBands[i].onDomainChange(1.0);
        // Recompute smoothing coefficient from current sample rate
        setSmoothingTimeMs(mSmoothMs);
    }

    void reset(Tv v = Tv(0)){
        for(unsigned i=0;i<NBands;++i) mBands[i].reset(v);
        mInGain  = mInGainT;
        mOutGain = mOutGainT;
        mBypassMix = mBypassT ? Tp(1) : Tp(0);
    }

    void setSmoothingTimeMs(Tp ms){
        mSmoothMs = ms;
        for(unsigned i=0;i<NBands;++i) mBands[i].setSmoothingTimeMs(ms);
        // also apply to strip gains/bypass
        Tp fs = Tp(Td::spu());
        if(fs <= Tp(0) || ms <= Tp(0)){ mA = Tp(1); return; }
        Tp tau = ms / Tp(1000);
        mA = Tp(1) - std::exp(Tp(-1) / (tau * fs));
        mA = std::clamp(mA, Tp(0), Tp(1));
    }

    // ----- global bypass -----
    void bypass(bool b){ mBypassT = b; }
    bool bypass() const { return mBypassT; }

    // ----- input/output gains (dB) -----
    void setInputGainDb(Tp db){
        mInGainDbT = db;
        mInGainT = dbToLin(db);
    }

    void setOutputGainDb(Tp db){
        mOutGainDbT = db;
        mOutGainT = dbToLin(db);
    }

    Tp inputGainDb()  const { return mInGainDbT; }
    Tp outputGainDb() const { return mOutGainDbT; }

    // ----- per-band control -----
    static constexpr unsigned bands(){ return NBands; }

    void setBand(unsigned i, const BandParams& p){
        if(i >= NBands) return;
        typename Band::Params bp;
        bp.mode = p.mode;
        bp.freqHz = p.freqHz;
        bp.Q = p.Q;
        bp.gainDb = p.gainDb;
        bp.bypass = p.bypass;
        mBands[i].setParams(bp);
    }

    BandParams band(unsigned i) const {
        BandParams p{};
        if(i >= NBands) return p;
        auto bp = mBands[i].params();
        p.mode = bp.mode;
        p.freqHz = bp.freqHz;
        p.Q = bp.Q;
        p.gainDb = bp.gainDb;
        p.bypass = bp.bypass;
        return p;
    }

    // Convenience setters
    void bandMode(unsigned i, Mode m){ auto p = band(i); p.mode=m; setBand(i,p); }
    void bandFreq(unsigned i, Tp f){ auto p = band(i); p.freqHz=f; setBand(i,p); }
    void bandQ   (unsigned i, Tp q){ auto p = band(i); p.Q=q; setBand(i,p); }
    void bandGainDb(unsigned i, Tp db){ auto p = band(i); p.gainDb=db; setBand(i,p); }
    void bandBypass(unsigned i, bool b){ auto p = band(i); p.bypass=b; setBand(i,p); }

    /// Process one sample through the full strip
    Tv operator()(Tv x){
        tickGlobals();

        // Global bypass mix (click-free)
        if(mBypassMix >= Tp(0.999))
            return x;

        Tv y = x * Tv(mInGain);

        for(unsigned i=0;i<NBands;++i)
            y = mBands[i](y);

        y *= Tv(mOutGain);

        return Tv(mBypassMix) * x + Tv(Tp(1) - mBypassMix) * y;
    }

private:
    void tickGlobals(){
        // Smooth global bypass + gains
        mInGain  = onePoleSmooth(mInGain,  mInGainT,  mA);
        mOutGain = onePoleSmooth(mOutGain, mOutGainT, mA);

        Tp bypassMixT = mBypassT ? Tp(1) : Tp(0);
        mBypassMix = onePoleSmooth(mBypassMix, bypassMixT, mA);
    }

private:
    Band mBands[NBands];

    // smoothing
    Tp mSmoothMs = Tp(10);
    Tp mA = Tp(1);

    // global gains targets/current
    Tp mInGainDbT  = Tp(0);
    Tp mOutGainDbT = Tp(0);
    Tp mInGainT  = Tp(1);
    Tp mOutGainT = Tp(1);
    Tp mInGain   = Tp(1);
    Tp mOutGain  = Tp(1);

    // global bypass (smoothed)
    bool mBypassT = false;
    Tp   mBypassMix = Tp(0); // 0 active, 1 bypass
};

} // namespace gam
