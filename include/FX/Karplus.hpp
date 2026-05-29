#pragma once
#include "GDSP_ModDelay.hpp"

class ModKarplus : public Function
{
public:
    ModKarplus()
    {
        setFreq(110.f);
        setDecay(0.98f);
        setDamping(0.4f);
        setPick(0.6f);
        setMix(1.0f);
    }

    void setFreq(float hz)
    {
        freq = std::max(10.f, hz);
        delaySamples = (size_t)(gam::sampleRate() / freq);
        buffer.assign(delaySamples, 0.f);
    }

    void setDecay(float d)    { decay = std::clamp(d, 0.f, 0.999f); }
    void setDamping(float d)
    {
        damping = std::clamp(d, 0.f, 1.f);
        float cutoff = 300.f + damping * 9000.f;
        lpf.setFreq(cutoff);
    }

    void setPick(float p) { pick = std::clamp(p, 0.f, 1.f); }
    void setMix(float m)  { mix  = std::clamp(m, 0.f, 1.f); }

    void trigger()
    {
        for (auto& s : buffer)
            s = (randf() * 2.f - 1.f) * pick;
    }

    float process(float input) override
    {
        float y = buffer[pos];

        float fb = lpf.process(y) * decay;

        buffer[pos] = fb;

        pos = (pos + 1) % buffer.size();

        return input * (1.f - mix) + y * mix;
    }

private:
    std::vector<float> buffer;
    size_t pos = 0;

    float freq = 110.f;
    float decay = 0.98f;
    float damping = 0.4f;
    float pick = 0.6f;
    float mix = 1.0f;

    OnePole lpf;
};
