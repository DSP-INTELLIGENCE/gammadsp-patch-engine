#pragma once
#include "LR4Crossover3Band.hpp"

class MultibandProcessor : public Function
{
public:
    MultibandProcessor(float sr = 48000.0f)
    : crossover(sr)
    {
        crossover.setFrequencies(200.0f, 2000.0f);
    }

    void setFrequencies(float lowMid, float midHigh)
    {
        crossover.setFrequencies(lowMid, midHigh);
    }

    void setBandProcessor(int band, BandProcessor* p)
    {
        processors[band] = p;
    }

    float process(float input) override
    {
        float low, mid, high;
        crossover.process(input, &low, &mid, &high);

        if (processors[0]) low  = processors[0]->process(low);
        if (processors[1]) mid  = processors[1]->process(mid);
        if (processors[2]) high = processors[2]->process(high);

        return low + mid + high;
    }

    void reset()
    {
        crossover.reset();
        for (auto* p : processors)
            if (p) p->reset();
    }

private:
    LR4Crossover3Band crossover;
    BandProcessor* processors[3] = { nullptr, nullptr, nullptr };
};
