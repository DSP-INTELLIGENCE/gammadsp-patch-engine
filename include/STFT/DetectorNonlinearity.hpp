#pragma once
#include <algorithm>
#include <cmath>


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
    void setDrive(float d) { drive = std::max((float)0, d); }
    void setPower(float p){ power = std::max((float)0.1, p); }
    void setAsymmetry(float a)
    {
        asym = std::clamp(a, (float)-1, (float)1);
    }

    void reset() {}

    // Input: rectified signal (>=0)
    inline float process(float x)
    {
        x = std::max(x, (float)0);

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
                return std::log((float)1 + drive * x);

            case Mode::Saturating:
                // forgiving detector (optical / tube)
                return std::tanh(drive * x);

            case Mode::Asymmetric:
            {
                // Fast rise, slow fall in detector domain
                float up   = std::tanh(drive * x);
                float down = std::pow(x, power);
                return (asym >= 0)
                    ? ((float)1 - asym) * down + asym * up
                    : ((float)1 + asym) * up   - asym * down;
            }
        }
        return x;
    }

private:
    Mode   mode  { Mode::Linear };
    float drive { (float)1 };
    float power { (float)1 };
    float asym  { (float)0 };
};
