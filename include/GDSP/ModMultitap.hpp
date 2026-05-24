#pragma once
#include <algorithm>
#include <vector>
#include "Multitap.hpp"
#include "Parameters.hpp"

class ModMultitap : public Function
{
public:
    struct Tap
    {
        float     baseDelay = 0.25f;   // seconds
        Modulator fbm;                 // feedback modulation
        Modulator mixm;                // tap level modulation
        float     lastOut = 0.0f;
    };

    ModMultitap(unsigned numTaps = 4, float maxDelaySeconds = 2.0f)
    : mDelay(maxDelaySeconds, numTaps)
    {
        setNumTaps(numTaps);
        setGlobalMix(1.0f);
        setAM(1.0f);
    }

    void setNumTaps(unsigned n)
    {
        n = std::max(1u, n);
        taps.resize(n);
        mDelay.setNumTaps(n);
    }

    unsigned getNumTaps() const { return (unsigned)taps.size(); }

    void setTapDelay(unsigned i, float sec)
    {
        if (i >= taps.size()) return;
        taps[i].baseDelay = std::max(sec, 0.0001f);
    }

    void setGlobalMix(float v) { mGlobalMix = std::clamp(v, 0.f, 1.f); }

    void setFBM(unsigned i, float v)
    {
        if (i >= taps.size()) return;
        taps[i].fbm.set(v);
    }

    void setMIXM(unsigned i, float v)
    {
        if (i >= taps.size()) return;
        taps[i].mixm.set(v);
    }

    void setAM(float v) { am.set(v); }

    float process(float input) override
    {
        float fbSum  = 0.0f;
        float wetSum = 0.0f;
        float mixSum = 0.0f;

        for (unsigned i = 0; i < taps.size(); ++i)
        {
            auto& t = taps[i];

            // 🔧 THIS WAS THE MISSING PIECE
            mDelay.setTapTime(i, t.baseDelay);

            float fb  = std::clamp(t.fbm.process(), -0.999f, 0.999f);
            float lvl = std::clamp(t.mixm.process(),  0.0f,  1.0f);

            float d = mDelay.read(i);

            fbSum  += d * fb;
            wetSum += d * lvl;
            mixSum += lvl;

            t.lastOut = d;
        }

        mDelay.write(input + fbSum);

        float wet = (mixSum > 1e-6f) ? (wetSum / mixSum) : 0.0f;
        float out = input * (1.0f - mGlobalMix) + wet * mGlobalMix;

        return out * am.process();
    }

    void run(const float* in, float* out, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }

private:
    MultitapDelay mDelay;
    std::vector<Tap> taps;
    float mGlobalMix = 1.0f;
    Modulator am;
};
