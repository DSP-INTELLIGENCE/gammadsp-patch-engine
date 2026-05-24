#pragma once
#include "GainController.hpp"
#include "AdaptiveTimeConstants.hpp"

template <class Sample>
class TimingResponseControls {
public:
    // ---- Base Times ----
    void setAttack(Sample sec)   { baseAttack = sec; update(); }
    void setRelease(Sample sec)  { baseRelease = sec; update(); }
    void setHold(Sample sec)     { gain.setHold(sec); }

    // ---- Curvature & Program Dependence ----
    void setAttackShape(typename AttackGenerator<Sample>::Shape s)
    { gain.setAttackShape(s); }

    void setAttackProgramDependence(Sample amount)
    { gain.setAttackProgramDependence(amount); }

    void setReleaseProgramDependence(Sample amount)
    { gain.setReleaseProgramDependence(amount); }

    // ---- Adaptive Timing ----
    void setAdaptiveAmount(Sample a) { adaptive.setAdaptAmount(a); }
    void setAdaptiveMemory(Sample m) { adaptive.setMemory(m); }

    // ---- State ----
    void reset(Sample initialDb = 0)
    {
        gain.reset(initialDb);
        adaptive.reset();
    }

    // ---- Main processing: smooth target gain dB ----
    Sample process(Sample targetDb)
    {
        Sample atk, rel;
        adaptive.compute(targetDb, gain.valueDb(), atk, rel);

        gain.setAttack(atk);
        gain.setRelease(rel);

        return gain.process(targetDb);
    }

    Sample valueDb() const { return gain.valueDb(); }

private:
    void update()
    {
        gain.setAttack(baseAttack);
        gain.setRelease(baseRelease);
    }

    Sample baseAttack  { (Sample)0.01 };
    Sample baseRelease { (Sample)0.1 };

    GainController<Sample>      gain;
    AdaptiveTimeConstants<Sample> adaptive;
};
