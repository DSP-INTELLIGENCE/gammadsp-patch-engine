#pragma once
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
    void setShiftMod(float v) { m_shift.set(v); }   // typically [-1,1]
    void setIM(float v) { m_im.set(v); }
    void setAM(float v) { m_am.set(v); }

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
        float in = m_im.process(x);
        if(p_in_tap) in = p_in_tap->process(in);
        in = dc.process(in);

        // Modulated shift (Hz). Use multiplicative mapping but keep it sane.
        float shift = m_shift.process(mShift);
        shift = std::clamp(shift, -0.45f * sr, 0.45f * sr);

        // Phase increment for complex oscillator
        const double dphi = (2.0 * M_PI) * (double)shift / (double)sr;
        phase += dphi;

        // Wrap phase to avoid growth
        if (phase >  M_PI) phase -= 2.0 * M_PI;
        if (phase < -M_PI) phase += 2.0 * M_PI;
        phase = m_phase.process(phase);
        // Analytic signal via Hilbert
        
        ComplexSample T = hilbert.process(in); // Gamma Hilbert: out in quadrature (approx)
        double i = T.real;
        double q = T.imag;
        // Complex multiply (real part)
        const double c = std::cos(phase);
        const double s = std::sin(phase);
        double y = i * c - q * s;

        // Mild safety (optional but recommended for feedback chains)
        y = std::tanh(y);
        if(p_out_tap) y = p_out_tap->process(y);
        return m_am.process(y);
    }

    StereoSample stereo(float x)
    {
        const float sr = gam::sampleRate();

        // Input gain + DC block (frequency shifters hate DC)
        float in = m_im.process(x);
        in = dc.process(in);

        // Modulated shift (Hz). Use multiplicative mapping but keep it sane.
        float shift = m_shift.process(mShift);
        shift = std::clamp(shift, -0.45f * sr, 0.45f * sr);

        // Phase increment for complex oscillator
        const double dphi = (2.0 * M_PI) * (double)shift / (double)sr;
        phase += dphi;

        // Wrap phase to avoid growth
        if (phase >  M_PI) phase -= 2.0 * M_PI;
        if (phase < -M_PI) phase += 2.0 * M_PI;
        phase = m_phase.process(phase);
        // Analytic signal via Hilbert
        
        ComplexSample T = hilbert.process(in); // Gamma Hilbert: out in quadrature (approx)
        double i = T.real;
        double q = T.imag;
        // Complex multiply (real part)
        const double c = std::cos(phase);
        const double s = std::sin(phase);
                
        return StereoSample(i*c,-q*s);
    }

    float mShift = 100.f;

    Modulator m_shift;
    Modulator m_phase;
    Modulator m_im, m_am;
    Function  *p_in_tap = nullptr;
    Function  *p_out_tap = nullptr;

private:
    double phase = 0.0;

    Hilbert hilbert; // << use Gamma’s stable Hilbert
    BlockDC dc;
};
