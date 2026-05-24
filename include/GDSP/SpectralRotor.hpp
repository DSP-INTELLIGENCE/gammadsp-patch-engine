#pragma once
#include <cmath>
#include <algorithm>

class SpectralRotor
{
public:
    SpectralRotor()
    {
        setRate(0.2f);
        setDepth(0.8f);
        setWidth(120.0f);
        setBaseShift(10.0f);
        setMix(1.0f);

        lfo.setWave(LFO::SINE);
        lfo.setUnipolar(true);

        shiftL.setMix(1.0f);
        shiftR.setMix(1.0f);

        shiftL.setIM(1.0f);
        shiftR.setIM(1.0f);

        shiftL.setAM(1.0f);
        shiftR.setAM(1.0f);
    }

    // ---------- Musical Controls ----------

    void setRate(float hz)
    {
        lfo.setFreq(std::max(0.001f, hz));
    }

    void setDepth(float d)
    {
        depth = std::clamp(d, 0.0f, 1.0f);
    }

    void setWidth(float hz)
    {
        width = std::max(0.0f, hz);
    }

    void setBaseShift(float hz)
    {
        baseShift = std::max(0.0f, hz);
    }

    void setMix(float m)
    {
        mix = std::clamp(m, 0.0f, 1.0f);
    }

    void reset()
    {
        lfo.reset();
    }

    // ---------- DSP ----------

    StereoSample process(float input)
    {
        // --- unified motion source (0..1) ---
        float m = lfo.process();
        float ctrl = depth * m;

        // --- spectral rotation ---
        float shift = baseShift + ctrl * width;

        shiftL.setShift(+shift);
        shiftR.setShift(-shift);

        float xl = shiftL.process(input);
        float xr = shiftR.process(input);

        // --- equal-power stereo rotation ---
        float angle = ctrl * 1.57079632679f;
        float gl = std::cos(angle);
        float gr = std::sin(angle);

        float wetL = xl * gl;
        float wetR = xr * gr;

        // --- wet/dry mix ---
        float dry = 1.0f - mix;
        float wet = mix;

        StereoSample out;
        out.outL = input * dry + wetL * wet;
        out.outR = input * dry + wetR * wet;

        return out;
    }

private:
    // ---------- Modulation ----------
    LFO lfo;

    float depth     = 0.8f;
    float width     = 120.0f;
    float baseShift = 10.0f;
    float mix       = 1.0f;

    // ---------- DSP ----------
    ModFreqShift shiftL;
    ModFreqShift shiftR;
};
