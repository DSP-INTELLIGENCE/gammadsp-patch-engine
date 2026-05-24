#pragma once
#include <cmath>
#include <algorithm>

class BarberPoleSpectralRotor : public Function
{
public:
    BarberPoleSpectralRotor()
    {
        setRate(0.1f);
        setDepth(1.0f);
        setBaseShift(20.0f);
        setWidth(120.0f);
        setMix(1.0f);
    }

    // ---------- Musical Controls ----------
    void setRate(float hz)        { rate.set(std::max(0.001f, hz)); }
    void setDepth(float d)        { depth.set(std::clamp(d, 0.f, 1.f)); }
    void setBaseShift(float hz)   { baseShift = std::max(0.f, hz); }
    void setWidth(float hz)       { width.set(std::max(0.f, hz)); }
    void setMix(float m)          { mix.set(std::clamp(m, 0.f, 1.f)); }
    void setShift(float s)        { shifter.set(std::clamp(s, 0.f, 1.f)); }
    void reset()
    {
        phase = 0.f;
        lastCtrl = 0.f;
        
    }

    // ---------- DSP ----------
    float process(float x) override
    {
        // --- unified rotation phase ---
        float r = rate.process();
        phase += r / (float)gam::sampleRate();
        if (phase >= 1.f) phase -= 1.f;

        // Saw gives constant upward motion
        float saw = 2.f * phase - 1.f;          // -1 .. +1

        // Control shaping
        float ctrl = (saw * depth.process() + 1.f) * 0.5f;
        ctrl = 0.98f * lastCtrl + 0.02f * ctrl;
        lastCtrl = ctrl;

        // --- frequency shift ---
        float shift = shifter.process(baseShift) + ctrl * width.process();
        shift = std::min(shift, 0.45f * (float)gam::sampleRate());

        shiftUp.freq(+shift);
        shiftDown.freq(-shift);

        float up   = shiftUp(x);
        float down = shiftDown(x);

        // --- barber-pole spectral window ---
        float wUp   = std::sin(ctrl * 3.14159265f);
        float wDown = std::sin((1.f - ctrl) * 3.14159265f);

        float spectral = up * wUp + down * wDown;

        // --- dry / wet ---
        float a = mix.process() * 1.5707963f;
        return x * std::cos(a) + spectral * std::sin(a);
    }

    Modulator rate;
    Modulator depth;
    Modulator width;
    Modulator mix;
    Modulator shifter;

private:
    // ---------- DSP ----------
    gam::FreqShift<double> shiftUp;
    gam::FreqShift<double> shiftDown;

    // ---------- Modulation ----------
    

    // ---------- Spectral Geometry ----------
    float baseShift = 20.0f;
    
    // ---------- State ----------
    float phase    = 0.f;
    float lastCtrl = 0.f;
};
