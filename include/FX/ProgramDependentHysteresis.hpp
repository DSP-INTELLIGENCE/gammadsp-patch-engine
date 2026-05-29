#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class ProgramDependentHysteresis {
public:
    void setBaseRange(Sample minAmt, Sample maxAmt)
    {
        baseMin = std::clamp(minAmt, (Sample)0, (Sample)0.99);
        baseMax = std::clamp(maxAmt, (Sample)0, (Sample)0.99);
    }

    void setEnergyMemory(Sample m) { energyMemory = std::clamp(m, (Sample)0, (Sample)0.999); }
    void setCurveRange(Sample lo, Sample hi)
    {
        curveLo = std::max((Sample)0.1, lo);
        curveHi = std::max(curveLo, hi);
    }

    void reset()
    {
        energy = 0;
        state = 0;
    }

    inline Sample process(Sample input, Sample env)
    {
        // Track long-term program energy
        energy = energy * energyMemory + env * ((Sample)1 - energyMemory);

        // Energy → hysteresis curve shape
        Sample curve = curveLo + (curveHi - curveLo) * energy;

        // Energy → hysteresis range
        Sample minAmt = baseMin * (Sample)(1 - energy * (Sample)0.5);
        Sample maxAmt = baseMax * (Sample)(1 - energy * (Sample)0.3);

        // Compute instantaneous hysteresis amount
        Sample dyn = minAmt + (maxAmt - minAmt) * 
                     ((Sample)1 - std::pow(env, curve));

        // Apply hysteresis
        state = state * dyn + input * ((Sample)1 - dyn);
        return state;
    }

private:
    Sample baseMin { 0.1 };
    Sample baseMax { 0.8 };

    Sample curveLo { 0.8 };
    Sample curveHi { 2.5 };

    Sample energyMemory { 0.999 };

    Sample energy { 0 };
    Sample state  { 0 };
};
