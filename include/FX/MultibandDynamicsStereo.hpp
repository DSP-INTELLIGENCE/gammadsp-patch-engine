// MultibandDynamicsStereo.hpp
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>

#include "CrossoverNetwork.hpp"          // LR4 split/recombine-safe (your earlier file)
#include "CharacterEngine.hpp"           // detector personality
#include "ThresholdComparator.hpp"       // soft/hard knee curve (your earlier file)
#include "GainController.hpp"            // smooth gain in dB (attack/hold/release)
#include "AdaptiveTimeConstants.hpp"     // optional program-dependent timing
#include "MorphGainStage.hpp"            // continuous morph gain element
#include "HysteresisProcessor.hpp"       // simple memory (you already have this)
#include "PeakDetector.hpp"
#include "ExpRMSFollower.hpp"

// -------- utilities --------
template <class Sample>
static inline Sample dbToLin(Sample db) { return std::pow((Sample)10, db / (Sample)20); }

template <class Sample>
static inline Sample linToDb(Sample x, Sample floorLin = (Sample)1e-12)
{
    x = std::max(x, floorLin);
    return (Sample)20 * std::log10(x);
}

// ============================================================
// Band processor: independent detector, hysteresis, timing, gain
// ============================================================
template <class Sample>
class BandDynamicsStereo {
public:
    enum class StereoLink { Unlinked, Max, Average, Rms };

    BandDynamicsStereo()
    {
        // sane defaults
        setMode(Downward);
        setThresholdDb((Sample)-24);
        setKneeDb((Sample)6);
        setRatio((Sample)4);

        setAttack((Sample)0.01);
        setRelease((Sample)0.10);
        setHold((Sample)0.0);

        setAdaptive((Sample)0.0, (Sample)0.9);

        setDetectorCharacter((Sample)0.25); // ~VCA-ish detector feel
        setDetectorHysteresis((Sample)0.0);

        setGainMorph((Sample)0.0);          // clean by default
        setMakeupDb((Sample)0);
        setDryWet((Sample)1);

        setStereoLink(StereoLink::Max);

        reset();
    }

    // ---- transfer curve ----
    void setMode(Mode m)            { mode = m; curve.setMode(m); }
    void setThresholdDb(Sample db)  { curve.setThreshold(db); }
    void setKneeDb(Sample db)       { curve.setKnee(std::max((Sample)0, db)); }
    void setRatio(Sample r)         { ratio = std::max((Sample)1, r); }

    // ---- timing ----
    void setAttack(Sample sec)      { baseAtk = std::max((Sample)1e-6, sec); }
    void setRelease(Sample sec)     { baseRel = std::max((Sample)1e-6, sec); }
    void setHold(Sample sec)        { gainCtrl.setHold(std::max((Sample)0, sec)); }

    void setAdaptive(Sample amount01, Sample memory01)
    {
        adaptAmount = std::clamp(amount01, (Sample)0, (Sample)1);
        adaptMemory = std::clamp(memory01, (Sample)0, (Sample)0.999);
        adapt.setAdaptAmount(adaptAmount);
        adapt.setMemory(adaptMemory);
        adapt.setBaseAttack(baseAtk);
        adapt.setBaseRelease(baseRel);
    }

    // ---- detector (independent per band) ----
    void setDetectorCharacter(Sample morph01) { detL.setMorph(morph01); detR.setMorph(morph01); }
    void setDetectorDrive(Sample d)           { detL.setDrive(d); detR.setDrive(d); }
    void setDetectorHysteresis(Sample a)      { hysL.setAmount(a); hysR.setAmount(a); }

    // ---- gain stage (independent per band) ----
    void setGainMorph(Sample morph01)         { gainStage.setMorph(morph01); }
    void setGainMorphSmoothing(Sample sec)    { gainStage.setMorphSmoothingSeconds(sec); }
    void setMakeupDb(Sample db)              { gainStage.setMakeupDb(db); }
    void setDryWet(Sample wet01)             { gainStage.setDryWet(wet01); }

    // ---- stereo linking (within band) ----
    void setStereoLink(StereoLink m) { link = m; }

    // ---- meters (per band) ----
    Sample gainReductionDb() const { return grDb; }
    Sample inRmsDb() const { return inRmsDbVal; }
    Sample outRmsDb() const { return outRmsDbVal; }
    Sample inPeakDb() const { return inPeakDbVal; }
    Sample outPeakDb() const { return outPeakDbVal; }

    void reset()
    {
        detL.reset(); detR.reset();
        hysL.reset(); hysR.reset();

        gainCtrl.reset((Sample)0);
        adapt.reset();

        inPeak.reset(); outPeak.reset();
        inRms.reset((Sample)0); outRms.reset((Sample)0);

        grDb = (Sample)0;
        inRmsDbVal = outRmsDbVal = (Sample)-120;
        inPeakDbVal = outPeakDbVal = (Sample)-120;
    }

    inline void process(Sample inL, Sample inR, Sample& outL, Sample& outR)
    {
        // -------- detector envelopes (linear) --------
        Sample envL = detL.process(inL);
        Sample envR = detR.process(inR);

        // detector hysteresis (memory)
        envL = hysL.process(envL);
        envR = hysR.process(envR);

        // stereo link envelope if requested
        Sample env = (Sample)0;
        switch (link)
        {
            case StereoLink::Unlinked: break;
            case StereoLink::Max:      env = std::max(envL, envR); envL = envR = env; break;
            case StereoLink::Average:  env = (envL + envR) * (Sample)0.5; envL = envR = env; break;
            case StereoLink::Rms:      env = std::sqrt((envL*envL + envR*envR) * (Sample)0.5); envL = envR = env; break;
        }

        // -------- transfer function → target gain (dB) --------
        // Use envelope in dB domain only for decision; curve.process expects linear env per your implementation.
        // curve.process returns a “correction amount” in dB-ish units; we apply ratio mapping like earlier.
        const Sample cL = curve.process(envL);
        const Sample cR = (link == StereoLink::Unlinked) ? curve.process(envR) : cL;

        auto targetFromC = [&](Sample c)->Sample
        {
            if (mode == Gate) {
                // c is positive attenuation needed below threshold
                return -std::max((Sample)0, c);
            }
            if (mode == Upward) {
                // comparator returns -c for upward in your code; treat magnitude as boost
                const Sample boost = std::abs(c) * ((Sample)1 - (Sample)1 / ratio);
                return +boost;
            }
            // Downward compression
            const Sample gr = std::max((Sample)0, c) * ((Sample)1 - (Sample)1 / ratio);
            return -gr;
        };

        const Sample targetDbL = targetFromC(cL);
        const Sample targetDbR = (link == StereoLink::Unlinked) ? targetFromC(cR) : targetDbL;

        // -------- adaptive timing (optional) --------
        adapt.setBaseAttack(baseAtk);
        adapt.setBaseRelease(baseRel);
        adapt.setAdaptAmount(adaptAmount);
        adapt.setMemory(adaptMemory);

        // We run ONE gain controller if linked; otherwise run two controllers (cheap & clean).
        Sample gainDbL = (Sample)0, gainDbR = (Sample)0;

        if (link != StereoLink::Unlinked)
        {
            Sample atk, rel;
            adapt.compute(targetDbL, gainCtrl.valueDb(), atk, rel);
            gainCtrl.setAttack(atk);
            gainCtrl.setRelease(rel);

            gainDbL = gainDbR = gainCtrl.process(targetDbL);
        }
        else
        {
            // two independent controllers (instantiate a second on demand)
            if (!hasR) { gainCtrlR = gainCtrl; hasR = true; } // copy settings/state on first use (simple)
            Sample atkL, relL, atkR, relR;
            adapt.compute(targetDbL, gainCtrl.valueDb(),  atkL, relL);
            adapt.compute(targetDbR, gainCtrlR.valueDb(), atkR, relR);
            gainCtrl.setAttack(atkL);   gainCtrl.setRelease(relL);
            gainCtrlR.setAttack(atkR);  gainCtrlR.setRelease(relR);
            gainDbL = gainCtrl.process(targetDbL);
            gainDbR = gainCtrlR.process(targetDbR);
        }

        const Sample gL = dbToLin<Sample>(gainDbL);
        const Sample gR = dbToLin<Sample>(gainDbR);

        grDb = std::max((Sample)0, -(link != StereoLink::Unlinked ? gainDbL : (gainDbL + gainDbR) * (Sample)0.5));

        // -------- gain element (continuous morph) --------
        outL = gainStage.process(inL, gL);
        outR = gainStage.process(inR, gR);

        // -------- meters (per band) --------
        const Sample inP  = std::max(std::abs(inL),  std::abs(inR));
        const Sample outP = std::max(std::abs(outL), std::abs(outR));

        inPeak.process(inP);
        outPeak.process(outP);

        inRms.process((inL + inR) * (Sample)0.5);
        outRms.process((outL + outR) * (Sample)0.5);

        inPeakDbVal  = linToDb<Sample>(inPeak.value());
        outPeakDbVal = linToDb<Sample>(outPeak.value());
        inRmsDbVal   = (Sample)10 * std::log10(std::max(inRms.energy(),  (Sample)1e-12));
        outRmsDbVal  = (Sample)10 * std::log10(std::max(outRms.energy(), (Sample)1e-12));
    }

private:
    // detector
    CharacterEngine<Sample> detL, detR;
    HysteresisProcessor<Sample> hysL, hysR;

    // transfer
    ThresholdComparator<Sample> curve;
    Mode mode = Downward;
    Sample ratio = (Sample)4;

    // timing
    GainController<Sample> gainCtrl;
    GainController<Sample> gainCtrlR; // only used if Unlinked
    bool hasR = false;

    AdaptiveTimeConstants<Sample> adapt;
    Sample baseAtk = (Sample)0.01;
    Sample baseRel = (Sample)0.10;
    Sample adaptAmount = (Sample)0.0;
    Sample adaptMemory = (Sample)0.9;

    // gain stage
    MorphGainStage<Sample> gainStage;

    // stereo link
    StereoLink link = StereoLink::Max;

    // meters
    PeakDetector<Sample> inPeak{ (Sample)0.001, (Sample)0.050 };
    PeakDetector<Sample> outPeak{ (Sample)0.001, (Sample)0.050 };
    ExpRMSFollower<Sample> inRms{ (Sample)0.300, (Sample)0.300 };
    ExpRMSFollower<Sample> outRms{ (Sample)0.300, (Sample)0.300 };

    Sample grDb = (Sample)0;
    Sample inRmsDbVal = (Sample)-120, outRmsDbVal = (Sample)-120;
    Sample inPeakDbVal = (Sample)-120, outPeakDbVal = (Sample)-120;
};

// ============================================================
// Full 3-band stereo multiband dynamics processor
// - LR4 crossover per channel
// - Independent per-band BandDynamicsStereo (detector/hys/timing/gain)
// ============================================================
template <class Sample>
class MultibandDynamicsStereo {
public:
    MultibandDynamicsStereo(Sample sampleRate,
                            Sample lowMidHz = (Sample)200,
                            Sample midHighHz = (Sample)3000)
    : sr(sampleRate), xo(sampleRate)
    {
        xo.setLowMidFreq(lowMidHz);
        xo.setMidHighFreq(midHighHz);
        reset();
    }

    // ---- crossover points (smooth these with MemoryLag if you automate) ----
    void setLowMidFreq(Sample hz)  { xo.setLowMidFreq(hz); }
    void setMidHighFreq(Sample hz) { xo.setMidHighFreq(hz); }

    // ---- per-band access ----
    BandDynamicsStereo<Sample>& low()  { return bandLow; }
    BandDynamicsStereo<Sample>& mid()  { return bandMid; }
    BandDynamicsStereo<Sample>& high() { return bandHigh; }

    void reset()
    {
        xo.reset();
        bandLow.reset();
        bandMid.reset();
        bandHigh.reset();
    }

    inline void processSample(Sample inL, Sample inR, Sample& outL, Sample& outR)
    {
        // --- split L ---
        Sample lLow, lMid, lHigh;
        xo.process3Band(inL, lLow, lMid, lHigh);

        // --- split R ---
        Sample rLow, rMid, rHigh;
        xo.process3Band(inR, rLow, rMid, rHigh);

        // --- per-band dynamics (independent) ---
        Sample oLL, oLR; bandLow.process(lLow,  rLow,  oLL, oLR);
        Sample oML, oMR; bandMid.process(lMid,  rMid,  oML, oMR);
        Sample oHL, oHR; bandHigh.process(lHigh, rHigh, oHL, oHR);

        // --- recombine (LR4 sums flat) ---
        outL = oLL + oML + oHL;
        outR = oLR + oMR + oHR;
    }

    void processBuffer(const Sample* inL, const Sample* inR,
                       Sample* outL, Sample* outR,
                       std::size_t n)
    {
        for (std::size_t i = 0; i < n; ++i)
            processSample(inL[i], inR[i], outL[i], outR[i]);
    }

private:
    Sample sr;
    CrossoverNetwork<Sample> xo;

    BandDynamicsStereo<Sample> bandLow;
    BandDynamicsStereo<Sample> bandMid;
    BandDynamicsStereo<Sample> bandHigh;
};

/*
### How you use it (quick example)

MultibandDynamicsStereo<float> mb(gam::sampleRate(), 180.0f, 3200.0f);

// Low band: gentle glue
mb.low().setThresholdDb(-30);
mb.low().setRatio(2.0f);
mb.low().setAttack(0.03f);
mb.low().setRelease(0.20f);
mb.low().setGainMorph(0.25f); // VCA-ish
mb.low().setDetectorHysteresis(0.3f);

// Mid band: vocal control
mb.mid().setThresholdDb(-24);
mb.mid().setRatio(3.0f);
mb.mid().setAttack(0.01f);
mb.mid().setRelease(0.12f);
mb.mid().setGainMorph(0.75f); // Optical-ish

// High band: de-ess / transient tame
mb.high().setThresholdDb(-28);
mb.high().setRatio(6.0f);
mb.high().setAttack(0.002f);
mb.high().setRelease(0.05f);
mb.high().setGainMorph(0.50f); // FET-ish

If you want, I can add:

* **per-band external key / sidechain EQ** (so highs can be keyed from a sibilance band)
* **per-band lookahead**
* **global stereo-link with selectable per-band linking** (common mastering behavior)
* **perfectly smooth crossover frequency modulation** using your `MemoryLag` blocks
*/