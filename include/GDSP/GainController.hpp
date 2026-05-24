#pragma once
#include <algorithm>
#include <cmath>
#include "AttackGenerator.hpp"
#include "ReleaseGenerator.hpp"
#include "HoldGenerator.hpp"

template <class Sample>
class GainController {
public:
    GainController() { reset(); }

    void setAttack(Sample sec)  { attackGen.setTime(sec); }
    void setRelease(Sample sec) { releaseGen.setTime(sec); }

    void setAttackShape(typename AttackGenerator<Sample>::Shape s)
    { attackGen.setShape(s); }

    void setAttackProgramDependence(Sample amount)
    { attackGen.setProgramDependence(amount); }

    void setReleaseProgramDependence(Sample amount)
    { releaseGen.setProgramDependence(amount); }

    void setHold(Sample sec)
    { releaseGen.setHold(sec); }

    void reset(Sample initialDb = 0.0f)
    {
        currentDb = initialDb;
        attackGen.reset(initialDb);
        releaseGen.reset(initialDb);
        holdGen.reset();
    }

    // Input: target gain in dB (negative for compression)
    // Output: smoothed gain in dB
    Sample process(Sample targetDb)
    {
        const bool releasing = (targetDb > currentDb + (Sample)1e-9);

        if (!releasing)
        {
            currentDb = attackGen.process(targetDb);
            holdGen.process(false);  // keep hold primed
        }
        else
        {
            if (holdGen.process(true))
            {
                // holding — freeze gain
            }
            else
            {
                currentDb = releaseGen.process(targetDb);
            }
        }
        return currentDb;
    }

    Sample valueDb() const { return currentDb; }

private:
    AttackGenerator<Sample>  attackGen;
    ReleaseGenerator<Sample> releaseGen;
    HoldGenerator<Sample>    holdGen;
    Sample currentDb = 0.0f;
};
