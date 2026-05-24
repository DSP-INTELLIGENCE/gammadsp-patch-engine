// Compressor.hpp
#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

// assumes these exist (from your earlier work)
#include "AttackGenerator.hpp"
#include "ReleaseGenerator.hpp"
#include "AdaptiveTimeConstants.hpp"
#include "AnalogVCA.hpp"
#include "TPT1Pole.hpp"

namespace gam {

template<class Tv>
static inline Tv dbToLin(Tv db){ return std::pow(Tv(10), db / Tv(20)); }

template<class Tv>
static inline Tv linToDb(Tv x){
    x = std::max(x, Tv(1e-20));
    return Tv(20) * std::log10(x);
}

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class Compressor : public Td {
public:
    enum class DetectorMode { Peak, RMS };

    Compressor(){
        reset();
        onDomainChange(1.0);
    }

    // ---------------- Parameters ----------------
    void thresholdDb(Tv db){ mThrDb = db; }
    void ratio(Tv r){ mRatio = std::max(r, Tv(1)); }
    void kneeDb(Tv db){ mKneeDb = std::max(db, Tv(0)); }

    void makeupDb(Tv db){ mMakeup = dbToLin(db); }
    void wet(Tv w){ mWet = std::clamp(w, Tv(0), Tv(1)); }

    // timing base
    void attack(Tv sec){ mBaseAtk = std::max(sec, Tv(1e-6)); }
    void release(Tv sec){ mBaseRel = std::max(sec, Tv(1e-6)); }

    // adaptive timing
    void adapt(Tv a){ mAdap.setAdaptAmount(a); }
    void memory(Tv m){ mAdap.setMemory(m); }
    void hold(Tv sec){ mRel.hold(std::max(sec, Tv(0))); }
    void releaseProgram(Tv p){ mRel.program(std::clamp(p, Tv(0), Tv(1))); }
    void attackShape(typename AttackGenerator<Tv,Td>::Shape s){ mAtk.shape(s); }

    // detector
    void detector(DetectorMode m){ mDetMode = m; }
    void detectorTime(Tv sec){ mDetTime = std::max(sec, Tv(1e-6)); updateDetCoeff(); }

    // sidechain HPF
    void sidechainHPHz(Tv hz){
        mSideHPHz = std::max(hz, Tv(0));
        updateSideHP();
    }
    void sidechainHPEnable(bool e){ mSideHPOn = e; }

    // VCA character
    void vcaModel(typename AnalogVCA<Tv,Td>::Model m){ mVCA.model(m); }
    void vcaSaturation(Tv s){ mVCA.saturation(s); }
    void vcaTHD(Tv d){ mVCA.thd(d); }
    void vcaBleed(Tv b){ mVCA.bleed(b); }
    void vcaAsymmetry(Tv a){ mVCA.asymmetry(a); }

    // metering
    Tv gainDb() const { return mSmoothedGainDb; }      // negative when compressing
    Tv envDb()  const { return mEnvDb; }               // detector envelope in dB
    Tv grDb()   const { return -mSmoothedGainDb; }     // positive GR

    // ---------------- Lifecycle ----------------
    void reset(){
        mEnv = Tv(0);
        mEnvDb = Tv(-120);
        mSmoothedGainDb = Tv(0);

        mAtk.reset(Tv(0));
        mRel.reset(Tv(0));
        mAdap.reset();

        mSideHP.reset(Tv(0));
        mVCA.reset();
    }

    void onDomainChange(double){
        updateDetCoeff();
        updateSideHP();

        mAtk.onDomainChange(1.0);
        mRel.onDomainChange(1.0);
        mAdap.onDomainChange(1.0);
        mVCA.onDomainChange(1.0);
    }

    // ---------------- Processing ----------------
    inline Tv operator()(Tv x){
        return (*this)(x, x);
    }

    /// x = audio input, sc = sidechain input (can be x)
    inline Tv operator()(Tv x, Tv sc){
        // --- sidechain filter (optional) ---
        Tv scf = mSideHPOn ? mSideHP.hp(sc) : sc;

        // --- detector envelope (peak or RMS) ---
        Tv level = Tv(0);
        if(mDetMode == DetectorMode::Peak){
            level = std::abs(scf);
        } else {
            // RMS: smooth power then sqrt
            level = scf * scf;
        }

        // one-pole smoothing in linear domain
        mEnv = (Tv(1) - mDetA) * level + mDetA * mEnv;

        Tv envLin = (mDetMode == DetectorMode::Peak)
            ? mEnv
            : std::sqrt(std::max(mEnv, Tv(0)));

        mEnvDb = linToDb(envLin);

        // --- gain computer (target gain in dB, <= 0) ---
        const Tv targetGainDb = computeGainDb(mEnvDb);

        // --- adaptive timing (compute attack/release times) ---
        mAdap.setBaseAttack(mBaseAtk);
        mAdap.setBaseRelease(mBaseRel);

        Tv atkTime, relTime;
        mAdap.compute(targetGainDb, mSmoothedGainDb, atkTime, relTime);

        // apply times (sample-rate safe via their onDomainChange hooks)
        mAtk.time(atkTime);
        mRel.time(relTime);

        // choose attack vs release path
        if(targetGainDb < mSmoothedGainDb){
            mSmoothedGainDb = mAtk(targetGainDb);
        } else {
            mSmoothedGainDb = mRel(targetGainDb);
        }

        // --- apply gain (linear) + VCA character + makeup + wet ---
        const Tv gLin = dbToLin(mSmoothedGainDb);
        Tv y = mVCA(x, gLin * mMakeup);

        // NOTE: AnalogVCA already has its own wet control; we do outer wet too if desired.
        // Keep one source of truth:
        return (Tv(1) - mWet) * x + mWet * y;
    }

private:
    // Soft-knee gain computer: returns gain in dB (<= 0)
    inline Tv computeGainDb(Tv inDb) const {
        const Tv thr = mThrDb;
        const Tv knee = mKneeDb;
        const Tv invRatioMinus1 = (Tv(1)/mRatio) - Tv(1); // negative or 0

        // no compression if ratio == 1
        if(mRatio <= Tv(1)) return Tv(0);

        if(knee <= Tv(0)){
            const Tv over = inDb - thr;
            if(over <= Tv(0)) return Tv(0);
            return invRatioMinus1 * over; // negative
        }

        const Tv half = knee * Tv(0.5);
        const Tv x = inDb;

        if(x <= thr - half) return Tv(0);
        if(x >= thr + half){
            const Tv over = x - thr;
            return invRatioMinus1 * over;
        }

        // inside knee: smooth quadratic
        // u in [0, knee]
        const Tv u = x - (thr - half);
        // GR = (1/R - 1) * u^2 / (2*knee)
        return invRatioMinus1 * (u * u) / (knee * Tv(2));
    }

    void updateDetCoeff(){
        const Tv fs = Tv(Td::spu());
        if(fs <= Tv(0)) return;
        // envelope smoothing coefficient for detector
        mDetA = std::exp(Tv(-1) / (mDetTime * fs));
    }

    void updateSideHP(){
        // If you want “off”, keep it configured anyway; hp() will be bypassed by flag.
        if(mSideHPHz > Tv(0)){
            mSideHP.freq(mSideHPHz);
        } else {
            // near-DC -> effectively bypass-ish
            mSideHP.freq(Tv(1e-4));
        }
    }

private:
    // -------- user params --------
    Tv mThrDb   { Tv(-18) };
    Tv mRatio   { Tv(4) };
    Tv mKneeDb  { Tv(6) };

    Tv mBaseAtk { Tv(0.01) };
    Tv mBaseRel { Tv(0.1) };

    Tv mMakeup  { Tv(1) };
    Tv mWet     { Tv(1) };

    DetectorMode mDetMode { DetectorMode::RMS };
    Tv mDetTime { Tv(0.01) }; // detector smoothing (not attack)

    bool mSideHPOn { false };
    Tv   mSideHPHz { Tv(80) };

    // -------- components --------
    AttackGenerator<Tv,Td>       mAtk;
    ReleaseGenerator<Tv,Td>      mRel;
    AdaptiveTimeConstants<Tv,Td> mAdap;

    AnalogVCA<Tv,Td>             mVCA;
    TPT1Pole<Tv,Td>              mSideHP;  // uses hp()

    // -------- detector state --------
    Tv mDetA { Tv(0.0) };
    Tv mEnv  { Tv(0) };
    Tv mEnvDb { Tv(-120) };

    // -------- gain state --------
    Tv mSmoothedGainDb { Tv(0) };
};

} // namespace gam
