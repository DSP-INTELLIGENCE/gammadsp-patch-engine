#pragma once
#include "SidechainInput.hpp"
#include "SidechainFilter.hpp"
#include "SidechainMonitor.hpp"
#include "SidechainMeter.hpp"

template <class Sample>
class SidechainSystem {
public:
    SidechainSystem(Sample sampleRate)
    : filter(sampleRate) {}

    // ---- Routing ----
    void setSource(typename SidechainInput<Sample>::Source s)
    { input.setSource(s); }

    void setMode(typename SidechainInput<Sample>::Mode m)
    { input.setMode(m); }

    void setBlend(Sample b)
    { input.setBlend(b); }

    // ---- Filtering ----
    void setHP(Sample hz)          { filter.setHP(hz); }
    void setLP(Sample hz)          { filter.setLP(hz); }
    void setTiltDb(Sample db)      { filter.setTiltDb(db); }
    void setTiltPivot(Sample hz)   { filter.setTiltPivot(hz); }
    void setTiltSlope(Sample s)    { filter.setTiltSlope(s); }

    // ---- Monitoring ----
    void enableMonitor(bool b)     { monitor.enable(b); }
    void setMonitorLevel(Sample g) { monitor.setLevel(g); }

    // ---- State ----
    void reset()
    {
        filter.reset();
        meter.reset();
    }

    // ---- Process ----
    inline Sample process(Sample inL, Sample inR,
                          Sample extL, Sample extR,
                          Sample& outL, Sample& outR)
    {
        Sample sc = input.process(inL, inR, extL, extR);
        sc = filter.process(sc);

        meter.process(sc);
        monitor.process(sc, outL, outR);

        return sc;
    }

    const SidechainMeter<Sample>& getMeter() const { return meter; }

private:
    SidechainInput<Sample>   input;
    SidechainFilter<Sample>  filter;
    SidechainMonitor<Sample> monitor;
    SidechainMeter<Sample>   meter;
};
