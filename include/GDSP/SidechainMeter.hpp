#pragma once
#include <cmath>
#include "PeakDetector.hpp"
#include "ExpRMSFollower.hpp"

template <class Sample>
class SidechainMeter {
public:
    void reset()
    {
        peak.reset();
        rms.reset();
        peakDb = rmsDb = (Sample)-120;
    }

    inline void process(Sample sc)
    {
        Sample p = peak.process(sc);
        Sample r = rms.process(sc);

        peakDb = (Sample)20 * std::log10(std::max(p, (Sample)1e-12));
        rmsDb  = (Sample)10 * std::log10(std::max(r * r, (Sample)1e-12));
    }

    Sample getPeakDb() const { return peakDb; }
    Sample getRmsDb()  const { return rmsDb; }

private:
    PeakDetector<Sample>   peak { (Sample)0.001, (Sample)0.050 };
    ExpRMSFollower<Sample> rms  { (Sample)0.300, (Sample)0.300 };

    Sample peakDb { (Sample)-120 };
    Sample rmsDb  { (Sample)-120 };
};
