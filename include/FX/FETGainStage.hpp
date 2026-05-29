#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class FETGainStage {
public:
    void setMakeupDb(Sample db) {
        makeup = std::pow((Sample)10, db / (Sample)20);
    }

    void setDryWet(Sample w) {
        wet = std::clamp(w, (Sample)0, (Sample)1);
    }

    void setDrive(Sample d) {
        drive = std::max((Sample)1, d);
    }

    void setAsymmetry(Sample a) {
        asym = std::clamp(a, (Sample)0, (Sample)1);
    }

    void reset() {}

    inline Sample process(Sample x, Sample controlGain)
    {
        // Input-dependent pre-shaping
        Sample g = controlGain;
        Sample y = x * g * makeup;

        // FET nonlinearity — asymmetric clipping
        Sample pos = std::tanh(y * drive);
        Sample neg = std::tanh(y * drive * (Sample)(1 - asym));
        y = (y >= 0) ? pos : neg;

        // Gain-dependent harmonic bloom
        Sample dist = (Sample)1 - g;
        y += dist * (Sample)0.15 * y * y * y;

        return ((Sample)1 - wet) * x + wet * y;
    }

private:
    Sample makeup { (Sample)1 };
    Sample wet    { (Sample)1 };

    Sample drive { (Sample)2 };
    Sample asym  { (Sample)0.4 };
};
