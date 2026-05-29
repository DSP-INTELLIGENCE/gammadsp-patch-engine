#pragma once
#include "RBJFilter.hpp"
#include "HysteresisProcessor.hpp"

template <class Sample>
class FreqDependentHysteresis {
public:
    FreqDependentHysteresis(Sample sampleRate)
    : lowFilter(sampleRate, RBJFilter<Sample>::LOWPASS),
      highFilter(sampleRate, RBJFilter<Sample>::HIGHPASS)
    {
        lowFilter.setFreq((Sample)250);
        highFilter.setFreq((Sample)2500);

        setLowAmount((Sample)0.7);
        setMidAmount((Sample)0.4);
        setHighAmount((Sample)0.15);
    }

    void setLowAmount(Sample a)  { lowH.setAmount(a); }
    void setMidAmount(Sample a)  { midH.setAmount(a); }
    void setHighAmount(Sample a) { highH.setAmount(a); }

    void reset()
    {
        lowFilter.reset();
        highFilter.reset();
        lowH.reset();
        midH.reset();
        highH.reset();
    }

    inline Sample process(Sample env)
    {
        // Split envelope into bands
        Sample low  = lowFilter.process(env);
        Sample high = highFilter.process(env);
        Sample mid  = env - low - high;

        // Apply different memory
        low  = lowH.process(low);
        mid  = midH.process(mid);
        high = highH.process(high);

        return low + mid + high;
    }

private:
    RBJFilter<Sample> lowFilter, highFilter;
    HysteresisProcessor<Sample> lowH, midH, highH;
};
