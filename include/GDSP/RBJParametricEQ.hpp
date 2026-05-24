#pragma once
#include "RBJFilter.hpp"

template<class Sample>
class ParametricEQBand {
public:
    using Type = typename RBJFilter<Sample>::Type;

    ParametricEQBand(Sample sr)
    : mFilter(sr, RBJFilter<Sample>::Type::PEAKING),
    mBypassMix(1.f) {}
    

    // ---- band config ----
    void enable(bool e) { mBypassMix.set(e ? 1.f : 0.f); }
    bool enabled() const { return mBypassMix.value() > 0.5f; }

    void setType(Type t) { mFilter.setType(t); }
    void setFreq(Sample f) { mFilter.setFreq(f); }
    void setQ(Sample q) { mFilter.setQ(q); }
    void setGainDb(Sample db) { mFilter.setGainDb(db); }
    void setShelfSlope(Sample s) { mFilter.setShelfSlope(s); }
    
    Sample process(Sample x) {
        Sample y = mFilter.process(x);
        Sample mix = mBypassMix.process();
        return x + (y - x) * mix;
    }

    void reset() { mFilter.reset(); }

private:
    RBJFilter<Sample>   mFilter;
    ParamLinear mBypassMix;
};


template<class Sample>
class ParametricEQ : public Function {
public:
    enum PresetLayout { LAYOUT_5_BAND = 5, LAYOUT_7_BAND = 7, LAYOUT_10_BAND = 10 };

    ParametricEQ(unsigned bands = LAYOUT_7_BAND)
    : mBypassMix(1.f), mInTrim(0.f), mOutTrim(0.f)
    {
        mBands.reserve(bands);
        for(unsigned i = 0; i < bands; ++i)
            mBands.emplace_back(gam::sampleRate());

        for(unsigned i = 0; i < bands; ++i) {
            mBands[i].enable(true);
            mBands[i].setType(RBJFilter<Sample>::Type::PEAKING);
            mBands[i].setFreq(200.f * std::pow(2.f, Sample(i)));
            mBands[i].setQ(0.707f);
            mBands[i].setGainDb(0.f);
            mBands[i].setShelfSlope(1.f);
        }
    }

    // ---- global controls ----
    void bypass(bool b) { mBypassMix.set(b ? 0.f : 1.f); }

    void setInputTrimDb(Sample db)  { mInTrim.set(db); }
    void setOutputTrimDb(Sample db) { mOutTrim.set(db); }

    // ---- DSP ----
    Sample process(Sample input) override {
        Sample mix = mBypassMix.process();

        Sample inG  = std::pow(10.f, mInTrim.process()  / 20.f);
        Sample outG = std::pow(10.f, mOutTrim.process() / 20.f);

        Sample x = input * inG;

        for(auto& b : mBands)
            x = b.process(x);

        x *= outG;

        return input + (x - input) * mix;
    }

    void run(const Sample* in, Sample* out, size_t n)  {
        for(size_t i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }

    void reset() {
        for(auto& b : mBands) b.reset();
    }

    unsigned numBands() const { return (unsigned)mBands.size(); }
    ParametricEQBand<Sample>& band(unsigned i) { return mBands[i]; }

private:

    std::vector<ParametricEQBand<Sample>> mBands;

    ParamLinear mBypassMix;
    ParamLinear mInTrim;
    ParamLinear mOutTrim;
};
