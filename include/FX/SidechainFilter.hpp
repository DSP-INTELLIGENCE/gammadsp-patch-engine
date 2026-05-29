#pragma once
#include "RBJFilter.hpp"
#include "TiltFilter.hpp"

template <class Sample>
class SidechainFilter {
public:
    SidechainFilter(Sample sampleRate)
    : hpf(sampleRate, RBJFilter<Sample>::HIGHPASS),
      lpf(sampleRate, RBJFilter<Sample>::LOWPASS),
      tilt(sampleRate)
    {
        hpf.setFreq((Sample)20);
        lpf.setFreq((Sample)20000);
    }

    // ---- HP / LP ----
    void setHP(Sample hz) { hpf.setFreq(hz); }
    void setLP(Sample hz) { lpf.setFreq(hz); }

    // ---- Tilt ----
    void setTiltDb(Sample db)      { tilt.setTiltDb(db); }
    void setTiltPivot(Sample hz)   { tilt.setPivot(hz); }
    void setTiltSlope(Sample slope){ tilt.setSlope(slope); }

    // ---- Process ----
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
