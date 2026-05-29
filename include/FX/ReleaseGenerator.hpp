// ReleaseGenerator.hpp
#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class ReleaseGenerator {
public:
    ReleaseGenerator()
    {
        setTime((Sample)0.1);
        setProgramDependence((Sample)0.0);
        setHold((Sample)0.0);
    }

    void setTime(Sample seconds)
    {
        baseTime = std::max((Sample)1e-6, seconds);
        updateBaseCoeff();
    }

    void setProgramDependence(Sample amount)
    {
        program = std::clamp(amount, (Sample)0, (Sample)1);
    }

    void setHold(Sample seconds)
    {
        holdTime = std::max((Sample)0, seconds);
        holdSamples = (int)(holdTime * gam::sampleRate());
    }

    void reset(Sample initialDb = 0)
    {
        valueDb = initialDb;
        holdCounter = 0;
    }

    Sample process(Sample targetDb)
    {
        if (targetDb < valueDb)
        {
            // Still compressing — do not release
            holdCounter = holdSamples;
            valueDb = targetDb;
            return valueDb;
        }

        // Releasing
        if (holdCounter > 0)
        {
            --holdCounter;
            return valueDb;
        }

        // Program dependent scaling
        Sample delta = std::abs(targetDb - valueDb);
        Sample scale = (Sample)1 + program * delta;
        Sample coeff = std::pow(baseCoeff, scale);

        valueDb = (1 - coeff) * targetDb + coeff * valueDb;
        return valueDb;
    }

    Sample value() const { return valueDb; }

private:
    void updateBaseCoeff()
    {
        baseCoeff = std::exp(-1 / (baseTime * gam::sampleRate()));
    }

    Sample baseTime { (Sample)0.1 };
    Sample baseCoeff { (Sample)0 };
    Sample program { (Sample)0 };

    Sample valueDb { 0 };
    Sample holdTime { 0 };
    int holdSamples { 0 };
    int holdCounter { 0 };
};
