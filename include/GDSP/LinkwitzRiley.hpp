#pragma once
#include "RBJFilter.hpp"

template <class Sample>
class LinkwitzRiley {
public:
    LinkwitzRiley(Sample sampleRate, Sample freq)
    : lp1(sampleRate, RBJFilter<Sample>::LOWPASS),
      lp2(sampleRate, RBJFilter<Sample>::LOWPASS),
      hp1(sampleRate, RBJFilter<Sample>::HIGHPASS),
      hp2(sampleRate, RBJFilter<Sample>::HIGHPASS)
    {
        setFreq(freq);
    }

    void setFreq(Sample f) {
        lp1.setFreq(f); lp2.setFreq(f);
        hp1.setFreq(f); hp2.setFreq(f);
    }

    void reset() {
        lp1.reset(); lp2.reset();
        hp1.reset(); hp2.reset();
    }

    inline void process(Sample x, Sample& low, Sample& high) {
        low  = lp2.process(lp1.process(x));
        high = hp2.process(hp1.process(x));
    }

private:
    RBJFilter<Sample> lp1, lp2, hp1, hp2;
};
