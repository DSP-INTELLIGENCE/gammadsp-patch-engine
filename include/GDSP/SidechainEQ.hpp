#pragma once
#include "RBJFilter.hpp"
#include "TiltFilter.hpp"

template <class Sample>
class SidechainEQ {
public:
    SidechainEQ(Sample sampleRate)
    : hpf(sampleRate, RBJFilter<Sample>::HIGHPASS),
      lpf(sampleRate, RBJFilter<Sample>::LOWPASS),
      tilt(sampleRate)
    {
        hpf.setFreq(Sample(20));
        lpf.setFreq(Sample(20000));
    }

    void setHP(Sample f) { hpf.setFreq(f); }
    void setLP(Sample f) { lpf.setFreq(f); }

    void setTiltDb(Sample db) { tilt.setTiltDb(db); }
    void setTiltPivot(Sample f) { tilt.setPivot(f); }
    void setTiltSlope(Sample s) { tilt.setSlope(s); }

    Sample process(Sample x)
    {
        x = hpf.process(x);
        x = lpf.process(x);
        x = tilt.process(x);
        return x;
    }

    void reset()
    {
        hpf.reset();
        lpf.reset();
        tilt.reset();
    }

private:
    RBJFilter<Sample> hpf, lpf;
    TiltFilter<Sample> tilt;
};
