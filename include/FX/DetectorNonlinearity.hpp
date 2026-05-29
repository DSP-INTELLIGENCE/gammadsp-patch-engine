#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class DetectorNonlinearity {
public:
    enum class Mode {
        Linear,       // env = x
        Power,        // env = x^p
        Logarithmic,  // env = log(1 + kx)
        Saturating,   // env = tanh(kx)
        Asymmetric    // different response up/down
    };

    void setMode(Mode m) { mode = m; }

    // General controls
    void setDrive(Sample d) { drive = std::max((Sample)0, d); }
    void setPower(Sample p){ power = std::max((Sample)0.1, p); }
    void setAsymmetry(Sample a)
    {
        asym = std::clamp(a, (Sample)-1, (Sample)1);
    }

    void reset() {}

    // Input: rectified signal (>=0)
    inline Sample process(Sample x)
    {
        x = std::max(x, (Sample)0);

        switch (mode)
        {
            case Mode::Linear:
                return x;

            case Mode::Power:
                // p < 1 → more transient sensitive
                // p > 1 → smoother, RMS-like
                return std::pow(x, power);

            case Mode::Logarithmic:
                // perceptual loudness
                return std::log((Sample)1 + drive * x);

            case Mode::Saturating:
                // forgiving detector (optical / tube)
                return std::tanh(drive * x);

            case Mode::Asymmetric:
            {
                // Fast rise, slow fall in detector domain
                Sample up   = std::tanh(drive * x);
                Sample down = std::pow(x, power);
                return (asym >= 0)
                    ? ((Sample)1 - asym) * down + asym * up
                    : ((Sample)1 + asym) * up   - asym * down;
            }
        }
        return x;
    }

private:
    Mode   mode  { Mode::Linear };
    Sample drive { (Sample)1 };
    Sample power { (Sample)1 };
    Sample asym  { (Sample)0 };
};
