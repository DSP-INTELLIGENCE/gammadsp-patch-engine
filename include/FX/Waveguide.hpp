#pragma once
#include "GDSP_ModDelay.hpp"

class Waveguide : public Function
{
public:
    Waveguide()
    {
        setFreq(110.f);
        setDecay(0.995f);
        setDamping(0.4f);
        setDispersion(0.2f);
        setDrive(0.05f);
    }

    void setFreq(float hz)
    {
        freq = std::max(10.f, hz);
        size_t delaySamples = (size_t)(gam::sampleRate() / freq / 2.f);
        fwd.assign(delaySamples, 0.f);
        bwd.assign(delaySamples, 0.f);
    }

    void setDecay(float d)     { decay = std::clamp(d, 0.f, 0.9999f); }
    void setDamping(float d)
    {
        damping = std::clamp(d, 0.f, 1.f);
        float cutoff = 300.f + damping * 12000.f;
        lpf.setFreq(cutoff);
    }

    void setDispersion(float d) { dispersion = std::clamp(d, 0.f, 1.f); }
    void setDrive(float d)      { drive = std::max(0.f, d); }

    void excite(float energy)
    {
        for (auto& s : fwd)
            s = (randf() * 2.f - 1.f) * energy;
    }

    float process(float) override
    {
        float out = fwd[pos] + bwd[pos];

        float reflectedFwd = lpf.process(bwd[pos]) * decay;
        float reflectedBwd = lpf.process(fwd[pos]) * decay;

        reflectedFwd = softClip(reflectedFwd, drive);
        reflectedBwd = softClip(reflectedBwd, drive);

        fwd[pos] = reflectedFwd;
        bwd[pos] = reflectedBwd;

        pos = (pos + 1) % fwd.size();

        return out * 0.5f;
    }

private:
    std::vector<float> fwd, bwd;
    size_t pos = 0;

    float freq = 110.f;
    float decay = 0.995f;
    float damping = 0.4f;
    float dispersion = 0.2f;
    float drive = 0.05f;

    OnePole lpf;
};
