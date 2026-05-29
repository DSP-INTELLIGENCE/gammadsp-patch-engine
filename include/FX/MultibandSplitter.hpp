#pragma once
#include "LinkwitzRileyFilter.hpp"

template <class Sample>
class MultibandSplitter {
public:
    MultibandSplitter(Sample sampleRate,
                      Sample lowMidFreq = 200,
                      Sample midHighFreq = 3000)
    : lowMid(sampleRate, lowMidFreq),
      midHigh(sampleRate, midHighFreq) {}

    void setLowMidFreq(Sample f)  { lowMid.setFreq(f); }
    void setMidHighFreq(Sample f) { midHigh.setFreq(f); }

    void reset() {
        lowMid.reset();
        midHigh.reset();
    }

    inline void process(Sample x,
                        Sample& low,
                        Sample& mid,
                        Sample& high)
    {
        Sample lmLow, lmHigh;
        lowMid.process(x, lmLow, lmHigh);

        Sample mhLow, mhHigh;
        midHigh.process(lmHigh, mhLow, mhHigh);

        low  = lmLow;
        mid  = mhLow;
        high = mhHigh;
    }
};
