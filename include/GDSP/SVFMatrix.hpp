// SVFMatrix.hpp
#pragma once
#include <vector>
#include <array>
#include "TVSVF.hpp"

class SVFMatrix
{
public:
    SVFMatrix(int outputs);

    void setSampleRate(float fs) { core.setSampleRate(fs); }
    void setCutoff(float hz)     { core.setCutoff(hz); }
    void setQ(float q)           { core.setQ(q); }

    void setMix(int out, float hp, float bp, float lp);

    void process(float x, float* outputs, int channel = 0);
    void reset() { core.reset(); }
    void processBlock(const float* in, float* out, int frames, int channel = 0);
    void processBlockMod(
        const float* in,
        const float* modHP,
        const float* modBP,
        const float* modLP,
        float* out,
        int frames,
        int channel = 0
    );

private:
    TVSVF core;
    std::vector<std::array<float,3>> M;
};
