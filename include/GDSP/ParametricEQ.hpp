#pragma once
#include "Biquad.hpp"

class ParametricEQ : public Function
{
public:
    
    ParametricEQ(size_t numBands = 5)
    {
        mBands.resize(numBands);
        for (auto& b : mBands)
            updateBand(b);
    }

    void setBand(size_t i, float freq, float q, float gainDB)
    {
        auto& b = mBands[i];
        b.freq = freq;
        b.q    = q;
        b.gain = gainDB;
        updateBand(b);
    }

    void bypassBand(size_t i, bool state)
    {
        mBands[i].bypass = state;
    }

    float process(float input) override
    {
        float x = input;
        for (auto& b : mBands)
            if (!b.bypass)
                x = b.filter.process(x);
        return x;
    }

    void reset()
    {
        for (auto& b : mBands)
            b.filter.reset();
    }

private:
    
    struct Band
    {
        Biquad filter;
        float freq  = 1000.0f;
        float q     = 0.707f;
        float gain  = 0.0f;   // dB
        bool  bypass = false;
    };

    std::vector<Band> mBands;

    void updateBand(Band& b)
    {
        b.filter.setType(gam::PEAKING);
        b.filter.setFreq(b.freq);
        b.filter.setRes(b.q);
        b.filter.setLevel(dbToLinear(b.gain));
    }

    float dbToLinear(float db)
    {
        return std::pow(10.0f, db / 20.0f);
    }
};
