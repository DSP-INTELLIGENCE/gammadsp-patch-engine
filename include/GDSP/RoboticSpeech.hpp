#pragma once
#include "VocalTract.hpp"
#include <cmath>

class RoboticSpeech : public Function
{
public:
    RoboticSpeech()
    {
        setPitch(120.f);
        setTone(0.6f);
        setArticulation(0.7f);
        setMix(1.f);
    }

    void setPitch(float p)        { pitch = std::clamp(p, 50.f, 400.f); }
    void setTone(float t)         { tone = std::clamp(t, 0.f, 1.f); }
    void setArticulation(float a) { artic = std::clamp(a, 0.f, 1.f); }
    void setMix(float m)          { mix = std::clamp(m, 0.f, 1.f); }

    float process(float x) override
    {
        // ----- Robotic carrier -----
        phase += pitch / (float)gam::sampleRate();
        if(phase >= 1.f) phase -= 1.f;

        float carrier = std::sin(phase * 6.2831853f);
        carrier = carrier * (1.f - tone) + std::tanh(carrier * 3.f) * tone;

        // ----- Articulation envelope -----
        float level = std::fabs(x);
        env += (level - env) * 0.01f;
        float voiced = carrier * env * artic + x * (1.f - artic);

        // ----- Vocal tract shaping -----
        float y = tract.process(voiced);

        // ----- Output mix -----
        return x * (1.f - mix) + y * mix;
    }

private:
    ModVocalTract tract;

    float pitch = 120.f;
    float tone  = 0.6f;
    float artic = 0.7f;
    float mix   = 1.f;

    float phase = 0.f;
    float env   = 0.f;
};
