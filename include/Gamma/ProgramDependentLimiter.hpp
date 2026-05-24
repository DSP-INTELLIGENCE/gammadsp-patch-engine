#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

// ------------------------------------------------------------
// Simple one-pole smoother for envelopes with separate time control
// y += alpha * (x - y)
// alpha = 1 - exp(-1/(time*fs))
// ------------------------------------------------------------
template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class OnePoleSmoother : public Td {
public:
    void time(Tv seconds){
        mTime = std::max(seconds, Tv(1e-6));
        update();
    }

    void reset(Tv v = Tv(0)){
        mY = v;
    }

    inline Tv operator()(Tv x){
        mY += mAlpha * (x - mY);
        return mY;
    }

    Tv value() const { return mY; }

    void onDomainChange(double){
        update();
    }

private:
    void update(){
        const Tv fs = Tv(Td::spu());
        if(fs <= Tv(0)) return;
        mAlpha = Tv(1) - std::exp(Tv(-1) / (mTime * fs));
    }

    Tv mTime  { Tv(0.01) };
    Tv mAlpha { Tv(0) };
    Tv mY     { Tv(0) };
};


// ------------------------------------------------------------
// Program-Dependent Limiter (v1)
// - No lookahead
// - Fast peak catching
// - Adaptive release based on sustained energy ("program")
// ------------------------------------------------------------
template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ProgramDependentLimiter : public Td {
public:
    // -------------------------------------------------
    // Parameters
    // -------------------------------------------------
    void thresholdDb(Tv db){ mThreshDb = db; }

    /// Base release time (seconds). Adaptive logic maps around this.
    void releaseBase(Tv s){ mRelBase = std::max(s, Tv(1e-4)); }

    /// Fastest release time (seconds) after isolated peaks.
    void releaseFast(Tv s){ mRelFast = std::max(s, Tv(1e-4)); }

    /// Slowest release time (seconds) under sustained program.
    void releaseSlow(Tv s){ mRelSlow = std::max(s, Tv(1e-4)); }

    /// Attack time (seconds) for peak detector (usually fixed very fast).
    void attackTime(Tv s){ mAtkTime = std::max(s, Tv(1e-5)); }

    /// Program analyzer time (seconds): higher = more “sustain-aware”.
    void programTime(Tv s){ mProgTime = std::max(s, Tv(1e-4)); }

    /// How strongly program controls release [0..1]
    void programAmount(Tv a){ mProgAmt = scl::clip(a, Tv(0), Tv(1)); }

    /// Reference level for program mapping (linear). Typical env range ~0..1.
    void programRef(Tv ref){ mProgRef = std::max(ref, Tv(1e-6)); }

    void makeupDb(Tv db){ mMakeup = std::pow(Tv(10), db / Tv(20)); }

    void mix(Tv m){ mMix = scl::clip(m, Tv(0), Tv(1)); }

    // -------------------------------------------------
    // Lifecycle
    // -------------------------------------------------
    ProgramDependentLimiter(){
        // sensible defaults for a mix-safe peak limiter
        mThreshDb  = Tv(-1.0);
        mAtkTime   = Tv(0.0005); // 0.5 ms
        mRelFast   = Tv(0.03);   // 30 ms
        mRelBase   = Tv(0.12);   // 120 ms
        mRelSlow   = Tv(0.35);   // 350 ms
        mProgTime  = Tv(0.08);   // 80 ms
        mProgAmt   = Tv(0.85);
        mProgRef   = Tv(0.25);
        mMakeup    = Tv(1);
        mMix       = Tv(1);

        onDomainChange(1.0);
        reset();
    }

    void reset(){
        mGrDb = Tv(0);
        mG    = Tv(1);

        mPeak.reset(Tv(0));
        mProg.reset(Tv(0));
        mGrAtk.reset(Tv(0));
        mGrRel.reset(Tv(0));
    }

    // -------------------------------------------------
    // Processing
    // -------------------------------------------------
    inline Tv operator()(Tv x){
        // --- hygiene (denorm guard)
        constexpr Tv DENORM = Tv(1e-18);
        if(std::abs(x) < DENORM) x = Tv(0);

        const Tv dry = x;

        // -------------------------------------------------
        // 1) Peak detector (fast attack)
        // -------------------------------------------------
        const Tv ax = std::abs(x);
        const Tv peakEnv = mPeak(ax);

        // -------------------------------------------------
        // 2) Program analyzer (slow envelope)
        // -------------------------------------------------
        const Tv progEnv = mProg(ax);

        // -------------------------------------------------
        // 3) Gain reduction target (∞:1)
        // -------------------------------------------------
        const Tv levelDb = linToDb(peakEnv);
        Tv grTargetDb = Tv(0);
        if(levelDb > mThreshDb){
            grTargetDb = (levelDb - mThreshDb); // limiter: full overage
        }

        // -------------------------------------------------
        // 4) Adaptive release time based on program
        // -------------------------------------------------
        // Map progEnv to [0..1]
        const Tv p = scl::clip(progEnv / mProgRef, Tv(0), Tv(1));

        // Blend between fast and slow release according to program,
        // then blend with base using programAmount.
        const Tv relProg = lerp(mRelFast, mRelSlow, p);
        const Tv relTime = lerp(mRelBase, relProg, mProgAmt);

        // Update release smoother time (SR-safe; small overhead, but fine v1)
        mGrRel.time(relTime);

        // -------------------------------------------------
        // 5) Smooth GR: fast attack, adaptive release
        // -------------------------------------------------
        Tv grDb;
        if(grTargetDb > mGrDb){
            grDb = mGrAtk(grTargetDb);
        } else {
            grDb = mGrRel(grTargetDb);
        }
        mGrDb = grDb;

        // -------------------------------------------------
        // 6) Apply gain
        // -------------------------------------------------
        const Tv g = dbToLin(-grDb);
        Tv y = x * g;

        // -------------------------------------------------
        // 7) Makeup + mix
        // -------------------------------------------------
        y *= mMakeup;
        Tv out = (Tv(1) - mMix) * dry + mMix * y;

        // --- NaN/Inf kill switch (prevents poisoning state)
        if(!std::isfinite(double(out))){
            reset();
            return Tv(0);
        }

        return out;
    }

    // -------------------------------------------------
    // Gamma domain hook
    // -------------------------------------------------
    void onDomainChange(double r){
        (void)r;

        // Peak and program envelopes
        mPeak.time(mAtkTime);     // fast peak follower
        mProg.time(mProgTime);    // program follower

        // GR smoothers:
        // Attack should be very fast for limiter behavior.
        mGrAtk.time(mAtkTime);

        // Release time is adaptive per sample; initialize with base.
        mGrRel.time(mRelBase);

        // propagate domain change to smoothers
        mPeak.onDomainChange(r);
        mProg.onDomainChange(r);
        mGrAtk.onDomainChange(r);
        mGrRel.onDomainChange(r);
    }

private:
    // -------------------------------------------------
    // Utilities
    // -------------------------------------------------
    static inline Tv linToDb(Tv x){
        return Tv(20) * std::log10(std::max(x, Tv(1e-12)));
    }

    static inline Tv dbToLin(Tv db){
        return std::pow(Tv(10), db / Tv(20));
    }

    static inline Tv lerp(Tv a, Tv b, Tv t){
        return a + (b - a) * t;
    }

private:
    // -------------------------------------------------
    // Internal smoothers
    // -------------------------------------------------
    OnePoleSmoother<Tv, Td> mPeak;   // fast peak envelope
    OnePoleSmoother<Tv, Td> mProg;   // slow "program" envelope

    OnePoleSmoother<Tv, Td> mGrAtk;  // fast attack for GR
    OnePoleSmoother<Tv, Td> mGrRel;  // adaptive release for GR

    // -------------------------------------------------
    // State
    // -------------------------------------------------
    Tv mGrDb { Tv(0) };
    Tv mG    { Tv(1) };

    // -------------------------------------------------
    // Params
    // -------------------------------------------------
    Tv mThreshDb { Tv(-1) };

    Tv mAtkTime  { Tv(0.0005) };

    Tv mRelFast  { Tv(0.03) };
    Tv mRelBase  { Tv(0.12) };
    Tv mRelSlow  { Tv(0.35) };

    Tv mProgTime { Tv(0.08) };
    Tv mProgAmt  { Tv(0.85) };
    Tv mProgRef  { Tv(0.25) };

    Tv mMakeup   { Tv(1) };
    Tv mMix      { Tv(1) };
};

} // namespace gam
