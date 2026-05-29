#pragma once
#include "LR4Crossover.hpp"

class MultibandCrossover
{
public:
    void setFrequencies(float lowMid, float midHigh)
    {
        xLowMid.setCutoff(lowMid);
        xMidHigh.setCutoff(midHigh);
    }

    void reset()
    {
        xLowMid.reset();
        xMidHigh.reset();
    }

    inline void process(float in, float& low, float& mid, float& high)
    {
        float lowMidHP;

        // First split: low / (mid+high)
        xLowMid.process(in, low, lowMidHP);

        // Second split: mid / high
        xMidHigh.process(lowMidHP, mid, high);
    }

private:
    LR4Crossover xLowMid;
    LR4Crossover xMidHigh;
};
