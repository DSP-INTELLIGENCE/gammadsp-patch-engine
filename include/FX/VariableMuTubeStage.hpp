#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class VariableMuTubeStage {
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

    void setWarmth(Sample w) {
        warmth = std::clamp(w, (Sample)0, (Sample)1);
    }

    void setSag(Sample s) {
        sag = std::clamp(s, (Sample)0, (Sample)1);
    }

    void reset()
    {
        bias = 0;
        sagEnv = 0;
    }

    // controlGain: linear gain from GainController
    inline Sample process(Sample x, Sample controlGain)
    {
        // Bias moves with gain reduction
        Sample gr = (Sample)1 - controlGain;
        bias += ((gr - bias) * (Sample)0.001);

        // Power-supply sag
        sagEnv += ((gr - sagEnv) * (Sample)0.0005);
        Sample sagMod = (Sample)1 - sag * sagEnv;

        // Tube core
        Sample v = x * drive * sagMod + bias;

        // Nonlinear transfer
        Sample y = std::tanh(v);

        // Harmonic bloom (even-order emphasis)
        y += warmth * (Sample)0.3 * y * y;

        y *= makeup;

        return ((Sample)1 - wet) * x + wet * y;
    }

private:
    Sample makeup { (Sample)1 };
    Sample wet    { (Sample)1 };

    Sample drive  { (Sample)1.5 };
    Sample warmth { (Sample)0.4 };
    Sample sag    { (Sample)0.3 };

    Sample bias   { 0 };
    Sample sagEnv { 0 };
};
