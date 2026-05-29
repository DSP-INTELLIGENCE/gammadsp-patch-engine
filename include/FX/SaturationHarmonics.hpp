#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class SaturationHarmonics {
public:
    void setDrive(Sample d)        { drive = std::max((Sample)0, d); }
    void setEven(Sample e)         { even = std::clamp(e, (Sample)0, (Sample)1); }
    void setOdd(Sample o)          { odd  = std::clamp(o, (Sample)0, (Sample)1); }
    void setAsymmetry(Sample a)    { asym = std::clamp(a, (Sample)-1, (Sample)1); }
    void setOutputTrimDb(Sample d) { trim = std::pow((Sample)10, d / (Sample)20); }

    void reset() {}

    inline Sample process(Sample x)
    {
        // Pre-drive
        Sample v = x * ((Sample)1 + drive * (Sample)4);

        // Asymmetry (even harmonics)
        v += asym * v * v;

        // Core soft saturation
        Sample y = std::tanh(v);

        // Odd harmonic enhancement
        y += odd * (Sample)0.2 * (y * y * y);

        // Even harmonic enhancement
        y += even * (Sample)0.2 * (y * y);

        // Output trim
        return y * trim;
    }

private:
    Sample drive { 0 };
    Sample even  { 0 };
    Sample odd   { 0 };
    Sample asym  { 0 };
    Sample trim  { 1 };
};
