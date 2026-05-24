#pragma once
#include "Biquad.hpp"

class GraphicEQ : public Function
{
public:
    GraphicEQ(const std::vector<float>& frequencies)
    {
        for (float f : frequencies)
            addBand(f);
    }

    void setGain(size_t band, float gainDB)
    {
        mBands[band].gain = gainDB;
        updateBand(band);
    }

    float getGain(size_t band) const
    {
        return mBands[band].gain;
    }

    size_t numBands() const { return mBands.size(); }

    float process(float input) override
    {
        float x = input;
        for (auto& b : mBands)
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
        float freq;
        float q = 1.0f;
        float gain = 0.0f;
    };

    std::vector<Band> mBands;

    void addBand(float freq)
    {
        Band b;
        b.freq = freq;
        b.filter.setType(gam::PEAKING);
        b.filter.setFreq(freq);
        b.filter.setRes(b.q);
        b.filter.setLevel(dbToLinear(b.gain));
        mBands.push_back(b);
    }

    void updateBand(size_t i)
    {
        auto& b = mBands[i];
        b.filter.setLevel(dbToLinear(b.gain));
    }

    float dbToLinear(float db)
    {
        return std::pow(10.0f, db / 20.0f);
    }
};
