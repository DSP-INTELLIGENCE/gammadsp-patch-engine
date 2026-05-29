#pragma once
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

class ModStereoTremolo : public Function
{
public:
    ModStereoTremolo()
    {
        setRate(4.0f);
        setDepth(0.7f);
        setShape(0.0f);   // sine → square
        setWidth(1.0f);   // full stereo
        setMix(1.0f);
    }

    // -------- Controls --------
    void setRate(float r)   { rate.set(std::max(0.01f, r)); }
    void setDepth(float d)  { depth.set(std::clamp(d, 0.f, 1.f)); }
    void setShape(float s)  { shape = std::clamp(s, 0.f, 1.f); }
    void setWidth(float w)  { width = std::clamp(w, 0.f, 1.f); }
    void setMix(float m)    { mix = std::clamp(m, 0.f, 1.f); }

    // -------- Audio --------
    void process(float inL, float inR, float& outL, float& outR)
    {
        // LFO phase
        phase += rate.process() / (float)gam::sampleRate();
        if (phase >= 1.f) phase -= 1.f;

        float s = std::sin(phase * 6.2831853f);

        // shape: sine → square
        float shaped = s * (1.f - shape) + std::copysign(1.f, s) * shape;

        float d = depth.process();

        // Stereo gains (opposed phase)
        float gL = 1.f - d * (0.5f - 0.5f * shaped);
        float gR = 1.f - d * (0.5f + 0.5f * shaped * width);

        float wetL = inL * gL;
        float wetR = inR * gR;

        outL = inL * (1.f - mix) + wetL * mix;
        outR = inR * (1.f - mix) + wetR * mix;
    }

private:
    Modulator rate, depth;

    float shape = 0.f;
    float width = 1.f;
    float mix   = 1.f;
    float phase = 0.f;
};
