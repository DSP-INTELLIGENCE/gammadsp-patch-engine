#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class LevelDependentHysteresis {
public:
    void setRange(Sample minAmt, Sample maxAmt)
    {
        minAmount = std::clamp(minAmt, (Sample)0, (Sample)0.99);
        maxAmount = std::clamp(maxAmt, (Sample)0, (Sample)0.99);
    }

    void setCurve(Sample c) { curve = std::max((Sample)0.1, c); }

    void reset(Sample v = 0)
    {
        state = v;
    }

    inline Sample process(Sample input, Sample env)
    {
        // env expected in [0..1]
        Sample e = std::clamp(env, (Sample)0, (Sample)1);

        // Compute dynamic hysteresis amount
        Sample dyn = minAmount + (maxAmount - minAmount) * 
                     ((Sample)1 - std::pow(e, curve));

        // Apply hysteresis
        state = state * dyn + input * ((Sample)1 - dyn);
        return state;
    }

private:
    Sample minAmount { 0.1 };
    Sample maxAmount { 0.8 };
    Sample curve { 1.5 };

    Sample state { 0 };
};
