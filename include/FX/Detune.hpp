#pragma once
#include "ModDelay.hpp"

class Detune
{
public:

    Detune()
    {
        L.setDelay(0.009f);
        R.setDelay(0.011f);

        setDepth(0.0008f);
        setRate(0.10f);
        setMix(0.6f);
        setWidth(1.0f);
        setTone(0.7f);
    }

    void setDepth(float d) { depth = std::clamp(d, 0.f, 0.003f); }
    void setRate(float r)  { rate.set(std::max(0.01f, r)); }
    void setMix(float m)   { mix = std::clamp(m, 0.f, 1.f); }
    void setWidth(float w) { width = std::clamp(w, 0.f, 1.f); }

    void setTone(float t)
    {
        tone = std::clamp(t, 0.f, 1.f);
        float cutoff = 2500.f + tone * 10000.f;
        lpfL.setFreq(cutoff);
        lpfR.setFreq(cutoff);
    }

    StereoSample process(float input)
    {
        // slow independent drift
        phase += rate.process() / (float)gam::sampleRate();
        if (phase > 1.f) phase -= 1.f;

        float drift = std::sin(phase * 6.2831853f) * depth;

        L.setDM(-drift);
        R.setDM(drift);

        float l = lpfL.process(L.process(input));
        float r = lpfR.process(R.process(input));

        float mid  = (l + r) * 0.5f;
        float side = (l - r) * 0.5f * width;

        float outL = mid + side;
        float outR = mid - side;

        outL = input * (1.f - mix) + outL * mix;
        outR = input * (1.f - mix) + outR * mix;

        return StereoSample(outL, outR);
    }

private:
    ModDelay L, R;
    Modulator rate;

    OnePole lpfL, lpfR;

    float depth = 0.0008f;
    float mix   = 0.6f;
    float width = 1.0f;
    float tone  = 0.7f;

    float phase = 0.f;
};
