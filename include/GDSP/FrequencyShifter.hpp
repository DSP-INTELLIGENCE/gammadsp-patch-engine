#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#include "Engine.hpp"
#include "Parameters.hpp"
#include "Hilbert.hpp"
#include "BlockDC.hpp"
#endif
#include <cmath>
#include <algorithm>




// Frequency shifter: analytic signal (Hilbert) + complex oscillator + complex multiply.
class FrequencyShifter : public Function {
public:
    FrequencyShifter(float shiftHz = 100.0f)
    : mShift(shiftHz)
    {
        setShift(shiftHz);
        reset();
    }

    void setShift(float hz) { mShift = hz; }
    void setShiftMod(float v) { mShiftMod.set(v); }   // typically [-1,1]
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    void reset()
    {
        phase = 0.0;
        hilbert.reset();
        dc.reset();
    }

    float process(float x) override
    {
        const float sr = gam::sampleRate();

        // Input gain + DC block (frequency shifters hate DC)
        float in = im.process(x);
        in = dc.process(in);

        // Modulated shift (Hz). Use multiplicative mapping but keep it sane.
        float shift = mShiftMod.process(mShift);
        shift = std::clamp(shift, -0.45f * sr, 0.45f * sr);

        // Phase increment for complex oscillator
        const double dphi = (2.0 * M_PI) * (double)shift / (double)sr;
        phase += dphi;

        // Wrap phase to avoid growth
        if (phase >  M_PI) phase -= 2.0 * M_PI;
        if (phase < -M_PI) phase += 2.0 * M_PI;

        // Analytic signal via Hilbert
        
        ComplexSample T = hilbert.process(in); // Gamma Hilbert: out in quadrature (approx)
        double i = T.real;
        double q = T.imag;
        // Complex multiply (real part)
        const double c = std::cos(phase);
        const double s = std::sin(phase);
        double y = i * c - q * s;

        // Mild safety (optional but recommended for feedback chains)
        // y = std::tanh(y);

        return am.process(y);
    }

private:
    float mShift = 100.f;
    Modulator mShiftMod;
    Modulator im, am;

    double phase = 0.0;

    Hilbert hilbert; // << use Gamma’s stable Hilbert
    BlockDC dc;
};
