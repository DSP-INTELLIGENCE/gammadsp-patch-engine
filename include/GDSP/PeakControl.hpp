#pragma once
#include "SoftClipper.hpp"
#include "HardClipper.hpp"
#include "PeakDetector.hpp"
#include <algorithm>
#include <cmath>

template <class Sample>
class PeakControl {
public:
    void setSoftDrive(Sample d)   { soft.setDrive(d); }
    void setSoftCeiling(Sample c){ soft.setCeiling(c); }
    void setSoftMix(Sample m)     { soft.setMix(m); }

    void setHardCeiling(Sample c) { hard.setCeiling(c); }
    void setHardMix(Sample m)     { hard.setMix(m); }

    void setLimiterCeiling(Sample db)
    {
        limiterCeiling = std::pow((Sample)10, db / (Sample)20);
    }

    void setLimiterRelease(Sample sec)
    {
        limiterRelease = std::max((Sample)0.01, sec);
        updateLimiter();
    }

    void reset()
    {
        soft.reset();
        hard.reset();
        limiter.reset();
    }

    inline Sample process(Sample x)
    {
        // 1. Soft clip first (tone & transient shaping)
        x = soft.process(x);

        // 2. Hard clip next (absolute safety)
        x = hard.process(x);

        // 3. Limiter safety (overshoot catcher)
        Sample peak = limiter.process(std::abs(x));
        if (peak > limiterCeiling)
        {
            Sample g = limiterCeiling / peak;
            x *= g;
        }

        return x;
    }

private:
    void updateLimiter()
    {
        limiter.setAttack((Sample)0.0001);
        limiter.setRelease(limiterRelease);
    }

    SoftClipper<Sample> soft;
    HardClipper<Sample> hard;
    PeakDetector<Sample> limiter;

    Sample limiterCeiling { (Sample)0.99 };
    Sample limiterRelease { (Sample)0.05 };
};
