#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include "GDSP_ModBiquad.hpp"
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

class ModalResonator : public Function
{
public:
    struct Mode {
        float freq;
        float decay;
        float gain;
    };

    ModalResonator(int modes = 8)
    : baseMix(0.7f)
    {
        setIM(1.f);
        setAM(1.f);

        resize(modes);
    }

    void resize(int modes)
    {
        filters.clear();
        params.clear();

        for(int i = 0; i < modes; ++i)
        {
            filters.emplace_back(1000.f, 0.9f, gam::BAND_PASS);
            params.push_back({ 200.f * (i+1), 0.995f, 1.f / (i+1) });
        }
    }

    // -------- Controls --------
    void setMode(int i, float freq, float decay, float gain)
    {
        if(i < 0 || i >= (int)filters.size()) return;
        params[i] = { freq, decay, gain };
    }

    void setMix(float m) { baseMix = std::clamp(m, 0.f, 1.f); }

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float x) override
    {
        float in = x * im.process();

        float y = 0.f;

        for(size_t i = 0; i < filters.size(); ++i)
        {
            auto& f = filters[i];
            auto& p = params[i];

            f.setType(gam::BAND_PASS);
            f.setCoefFromFreq(p.freq, p.decay);

            y += f.process(in) * p.gain;
        }

        float out = x * (1.f - baseMix) + y * baseMix;
        out *= am.process();

        return out;
    }

private:
    std::vector<ModBiquad> filters;
    std::vector<Mode> params;

    float baseMix;

    Modulator im, am;
};
