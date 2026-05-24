// DriveStage.hpp
#pragma once

#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
template<class T>
static inline T dbToLin(T db){
    return std::pow(T(10), db / T(20));
}

template<class T>
static inline T clamp01(T v){
    return std::clamp(v, T(0), T(1));
}

template<class T>
static inline T clampHz(T v){
    return std::max(T(0), v);
}

// ------------------------------------------------------------
// One-pole time-based smoother
// y += a*(x - y), a = 1 - exp(-1/(tau*fs))
// ------------------------------------------------------------
template<class T>
class Smooth1Pole {
public:
    void reset(T v){ value = target = v; }

    void setTimeSeconds(T sec, T fs){
        sec = std::max(sec, T(1e-6));
        fs  = std::max(fs,  T(1));
        alpha = T(1) - std::exp(T(-1) / (sec * fs));
    }

    void setTarget(T t){ target = t; }

    inline T process(){
        value += alpha * (target - value);
        return value;
    }

    T value  = T(0);
    T target = T(0);
    T alpha  = T(0.001);
};

// ------------------------------------------------------------
// TPT 1-pole lowpass / highpass (ZDF / trapezoidal integrator)
// Uses the same math you already use elsewhere.
// ------------------------------------------------------------
template<class T>
class TPT1Pole {
public:
    TPT1Pole(T fc = T(1000)){ freq(fc, T(44100)); reset(T(0)); }

    void reset(T v = T(0)){ s = v; }

    void freq(T fc, T fs){
        fs = std::max(fs, T(1));
        fc = std::clamp(fc, T(1e-4), T(0.49) * fs);
        mFc = fc; mFs = fs;

        const T pi = T(3.14159265358979323846);
        g = std::tan(pi * fc / fs);
        a = g / (T(1) + g);
    }

    inline T lp(T x){
        const T v = (x - s) * a;
        const T y = v + s;
        s = y + v;
        return y;
    }

    inline T hp(T x){
        const T ylp = lp(x);
        return x - ylp;
    }

    T g = T(0), a = T(0), s = T(0);
    T mFc = T(1000), mFs = T(44100);
};

// ------------------------------------------------------------
// Envelope follower (peak) with attack/release times
// env rises with attack, falls with release.
// ------------------------------------------------------------
template<class T>
class EnvFollowerAR {
public:
    void reset(T v = T(0)){ env = v; }

    void setTimes(T attackSec, T releaseSec, T fs){
        fs = std::max(fs, T(1));
        attackSec  = std::max(attackSec,  T(1e-6));
        releaseSec = std::max(releaseSec, T(1e-6));
        aAtk = T(1) - std::exp(T(-1) / (attackSec  * fs));
        aRel = T(1) - std::exp(T(-1) / (releaseSec * fs));
    }

    inline T operator()(T xAbs){
        const T a = (xAbs > env) ? aAtk : aRel;
        env += a * (xAbs - env);
        return env;
    }

    T env = T(0);

private:
    T aAtk = T(0.01);
    T aRel = T(0.001);
};

// ------------------------------------------------------------
// DriveStage
// Linear-only gain staging + tone conditioning + dynamic drive
// Intended to sit BEFORE WaveshaperCore.
// ------------------------------------------------------------
template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class DriveStage : public Td {
public:
    enum class DynMode : unsigned {
        MoreDriveWithLevel = 0, // loud => more drive (aggressive)
        LessDriveWithLevel = 1  // loud => less drive (self-regulating)
    };

    DriveStage(){
        onDomainChange(1.0);
    }

    // -------------------
    // Lifecycle
    // -------------------
    void reset(){
        // smoothers
        smInDb.reset(inGainDb);
        smDriveDb.reset(driveDb);
        smHeadDb.reset(headroomDb);
        smTrimDb.reset(outTrimDb);

        smHpHz.reset(hpHz);
        smLpHz.reset(lpHz);
        smTiltDb.reset(tiltDb);

        smDynAmt.reset(dynAmount);
        smAuto.reset(autoGain);
        smSafety.reset(safety);

        // filters + env
        hp.reset(Tv(0));
        lp.reset(Tv(0));
        tiltHP.reset(Tv(0));
        tiltLP.reset(Tv(0));

        env.reset(Tv(0));
    }

    void onDomainChange(double){
        const Tv fs = Tv(Td::spu());
        if(fs <= Tv(0)) return;

        // smoothing times (tune)
        const Tv tGain = Tv(0.020);
        const Tv tTone = Tv(0.040);
        const Tv tDyn  = Tv(0.040);

        smInDb.setTimeSeconds(tGain, fs);
        smDriveDb.setTimeSeconds(tGain, fs);
        smHeadDb.setTimeSeconds(tGain, fs);
        smTrimDb.setTimeSeconds(tGain, fs);

        smHpHz.setTimeSeconds(tTone, fs);
        smLpHz.setTimeSeconds(tTone, fs);
        smTiltDb.setTimeSeconds(tTone, fs);

        smDynAmt.setTimeSeconds(tDyn, fs);
        smAuto.setTimeSeconds(tGain, fs);
        smSafety.setTimeSeconds(tGain, fs);

        // envelope times
        env.setTimes(envAtkSec, envRelSec, fs);

        // init tone filters at defaults
        updateToneFilters(fs);

        reset();
    }

    // -------------------
    // Params (targets)
    // -------------------
    void setInputGainDb(Tv db){ inGainDb = db; smInDb.setTarget(db); }
    void setDriveDb(Tv db){ driveDb = db; smDriveDb.setTarget(db); }
    void setHeadroomDb(Tv db){
        headroomDb = std::max(Tv(-60), db); // allow small headroom but not insane
        smHeadDb.setTarget(headroomDb);
    }
    void setOutputTrimDb(Tv db){ outTrimDb = db; smTrimDb.setTarget(db); }

    void setHPHz(Tv hz){ hpHz = clampHz(hz); smHpHz.setTarget(hpHz); }
    void setLPHz(Tv hz){ lpHz = clampHz(hz); smLpHz.setTarget(lpHz); }
    void setTiltDb(Tv db){ tiltDb = db; smTiltDb.setTarget(db); }

    void setDynAmount(Tv a){ dynAmount = clamp01(a); smDynAmt.setTarget(dynAmount); }
    void setDynMode(DynMode m){ dynMode = m; }

    void setEnvAttack(Tv sec){ envAtkSec = std::max(Tv(1e-4), sec); refreshEnvTimes(); }
    void setEnvRelease(Tv sec){ envRelSec = std::max(Tv(1e-4), sec); refreshEnvTimes(); }

    // 0..1 blend
    void setAutoGain(Tv on01){ autoGain = clamp01(on01); smAuto.setTarget(autoGain); }

    // 0..1 blend
    void setSafety(Tv on01){ safety = clamp01(on01); smSafety.setTarget(safety); }

    // Adjust the envelope reference level (linear)
    // Default ~0.25 (-12 dBFS) is a good musical pivot.
    void setEnvRef(Tv ref){ envRef = std::max(Tv(1e-6), ref); }

    // Optional: cap dynamic normalization
    void setEnvCap(Tv cap){ envCap = std::max(Tv(1), cap); }

    // -------------------
    // Processing
    // -------------------
    inline Tv operator()(Tv x){
        const Tv fs = Tv(Td::spu());
        (void)fs; // may be unused if domain isn't set yet

        // Smooth parameter values
        const Tv inDb   = smInDb.process();
        const Tv drvDb  = smDriveDb.process();
        const Tv headDb = smHeadDb.process();
        const Tv trimDb = smTrimDb.process();

        const Tv hpHzS  = smHpHz.process();
        const Tv lpHzS  = smLpHz.process();
        const Tv tiltDbS= smTiltDb.process();

        const Tv dynAmt = smDynAmt.process();
        const Tv ag     = smAuto.process();
        const Tv saf    = smSafety.process();

        // Update tone filters (cheap enough; if you want, only update when changed)
        updateToneFilters(Tv(Td::spu()), hpHzS, lpHzS, tiltDbS);

        // 1) Input trim (linear)
        const Tv inGainLin = dbToLin(inDb);

        // 2) Drive mapping (linear)
        const Tv driveLin = dbToLin(drvDb);

        // 3) Headroom normalization (linear)
        // headroomLin is "how much bigger than nominal" before you want to really hit the shaper
        const Tv headroomLin = dbToLin(headDb);
        const Tv headInv = (headroomLin > Tv(1e-12)) ? (Tv(1) / headroomLin) : Tv(1);

        Tv y = x * inGainLin;
        y *= driveLin * headInv;

        // 4) Pre-emphasis tone shaping (linear)
        // HP then LP, with optional tilt
        y = hp.hp(y);
        y = lp.lp(y);

        if(std::abs(tiltDbS) > Tv(1e-6)){
            // Tilt using HP-LP emphasis around mid
            // tiltDb +/-12 is reasonable; map to linear gain
            const Tv k = std::clamp(tiltDbS / Tv(12), Tv(-1), Tv(1));
            const Tv hi = tiltHP.hp(y);
            const Tv lo = tiltLP.lp(y);
            y = y + k * (hi - lo);
        }

        // 5) Envelope follower
        const Tv e = env(std::abs(y));

        // normalize env around envRef and cap
        Tv en = e / envRef;
        en = std::clamp(en, Tv(0), envCap);

        // 6) Dynamic drive modulation
        // dynAmt is 0..1. en is 0..envCap.
        Tv dynGain = Tv(1);
        if(dynAmt > Tv(0)){
            if(dynMode == DynMode::MoreDriveWithLevel){
                dynGain = Tv(1) + dynAmt * en;
            } else {
                dynGain = Tv(1) / (Tv(1) + dynAmt * en);
            }
        }
        y *= dynGain;

        // 7) Auto-gain heuristic (optional blend)
        // Keep it conservative: derived from total gain above unity.
        // totalDrive roughly = driveLin * dynGain / headroomLin
        const Tv totalDrive = std::max(Tv(0), driveLin * dynGain * headInv);
        const Tv comp = autoComp(totalDrive); // <= 1 as drive increases
        y *= (Tv(1) - ag) + ag * comp;

        // 8) Output trim (still linear)
        y *= dbToLin(trimDb);

        // 9) Safety (optional) — gentle soft limiting
        if(saf > Tv(0)){
            const Tv lim = softLimit(y);
            y = y * (Tv(1) - saf) + lim * saf;
        }

        return y;
    }

private:
    // ---- internals ----

    void refreshEnvTimes(){
        const Tv fs = Tv(Td::spu());
        if(fs > Tv(0)){
            env.setTimes(envAtkSec, envRelSec, fs);
        }
    }

    void updateToneFilters(Tv fs){
        updateToneFilters(fs, hpHz, lpHz, tiltDb);
    }

    void updateToneFilters(Tv fs, Tv hpHzS, Tv lpHzS, Tv tiltDbS){
        if(fs <= Tv(0)) return;

        // clamp reasonable ranges
        const Tv nyq = fs * Tv(0.5);
        const Tv hpC = std::clamp(hpHzS, Tv(0), nyq * Tv(0.99));
        const Tv lpC = std::clamp(lpHzS, Tv(0), nyq * Tv(0.99));

        // if LP is set below HP by accident, nudge it
        Tv lpUse = lpC;
        if(lpUse > Tv(0) && lpUse < hpC) lpUse = hpC;

        // HP: if 0 Hz, effectively bypass (set very low)
        hp.freq(std::max(hpC, Tv(1e-4)), fs);

        // LP: if 0 Hz, bypass by setting near Nyquist
        lp.freq((lpUse > Tv(0)) ? lpUse : (nyq * Tv(0.99)), fs);

        // Tilt helpers: pick fixed corners
        // These are inexpensive and musical; you can expose them later.
        (void)tiltDbS;
        tiltHP.freq(Tv(3000), fs);
        tiltLP.freq(Tv(300),  fs);
    }

    static inline Tv autoComp(Tv totalDrive){
        // unity at 1, decreases gently as drive grows
        const Tv d = std::max(Tv(0), totalDrive - Tv(1));
        return Tv(1) / (Tv(1) + Tv(0.35) * d);
    }

    static inline Tv softLimit(Tv x){
        // gentle limiter: x / (1 + |x|)
        return x / (Tv(1) + std::abs(x));
    }

private:
    // ---- user targets ----
    Tv inGainDb   = Tv(0);     // 0 dB
    Tv driveDb    = Tv(0);     // 0 dB
    Tv headroomDb = Tv(12);    // +12 dB headroom
    Tv outTrimDb  = Tv(0);

    Tv hpHz       = Tv(0);     // 0 = bypass-ish
    Tv lpHz       = Tv(0);     // 0 = bypass-ish
    Tv tiltDb     = Tv(0);     // +/- dB

    Tv dynAmount  = Tv(0);
    DynMode dynMode = DynMode::MoreDriveWithLevel;

    Tv autoGain   = Tv(0);     // 0..1
    Tv safety     = Tv(0);     // 0..1

    // envelope params
    Tv envAtkSec  = Tv(0.005);
    Tv envRelSec  = Tv(0.100);
    Tv envRef     = Tv(0.25);  // ~ -12 dBFS pivot
    Tv envCap     = Tv(8);     // cap normalized env

    // ---- smoothers ----
    Smooth1Pole<Tv> smInDb, smDriveDb, smHeadDb, smTrimDb;
    Smooth1Pole<Tv> smHpHz, smLpHz, smTiltDb;
    Smooth1Pole<Tv> smDynAmt, smAuto, smSafety;

    // ---- tone filters ----
    TPT1Pole<Tv> hp;      // use hp.hp()
    TPT1Pole<Tv> lp;      // use lp.lp()
    TPT1Pole<Tv> tiltHP;  // hp for tilt
    TPT1Pole<Tv> tiltLP;  // lp for tilt

    // ---- envelope ----
    EnvFollowerAR<Tv> env;
};

} // namespace gam
