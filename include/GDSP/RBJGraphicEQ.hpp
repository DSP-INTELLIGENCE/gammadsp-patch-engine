#pragma once
#include "RBJFilter.hpp"
#include "Parameters.hpp"
#include <vector>

template<class Sample>
class GraphicEQBand {
public:
    GraphicEQBand(Sample sampleRate, Sample centerFreq, Sample Q)
    : mFreq(centerFreq),
      mQ(Q),
      mGain(0.f),
      mFilter(sampleRate, RBJFilter::Type::PEAKING, centerFreq, Q, 0.f)
    {}

    void setGainDb(Sample db) { mGain.set(db); }

    Sample gainDb() const { return mGain.value(); }
    Sample freq() const { return mFreq; }

    void reset() { mFilter.reset(); }

    Sample process(Sample x)
    {
        const Sample g = mGain.process();

        if (mGain.isRamping())
            mFilter.setGainDb(g);

        return mFilter.process(x);
    }


private:
    Sample mFreq;
    Sample mQ;

    ParamLinear mGain;
    RBJFilter   mFilter;
};

template<class Sample>
class GraphicEQ : public Function {
public:
    GraphicEQ(Sample sampleRate, const std::vector<Sample>& freqs, Sample Q)
    : mSR(sampleRate)
    {
        mBands.reserve(freqs.size());
        for (Sample f : freqs)
            mBands.emplace_back(sampleRate, f, Q);
    }

    size_t numBands() const { return mBands.size(); }

    GraphicEQBand& band(size_t i) { return mBands[i]; }

    void setInputTrimDb(Sample db)  { mInTrim.set(db); }
    void setOutputTrimDb(Sample db) { mOutTrim.set(db); }

    void bypass(bool b) { mBypassMix.set(b ? 0.f : 1.f); }

    void reset()
    {
        for (auto& b : mBands) b.reset();
    }

    Sample process(Sample input) override
    {
        const Sample dry = input;

        Sample x = input * db_to_lin(mInTrim.process());

        for (auto& b : mBands)
            x = b.process(x);

        x *= db_to_lin(mOutTrim.process());

        const Sample mix = mBypassMix.process();
        return dry + (x - dry) * mix;
    }

private:
    static inline Sample db_to_lin(Sample db)
    {
        return std::pow(10.f, db / 20.f);
    }

private:
    Sample mSR;
    std::vector<GraphicEQBand> mBands;

    ParamLinear mInTrim    { 0.f };
    ParamLinear mOutTrim   { 0.f };
    ParamLinear mBypassMix { 1.f };
};
