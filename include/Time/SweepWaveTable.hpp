#include "GDSP_Oscillators.hpp"
class Wavetable {
public:
    explicit Wavetable(size_t size = 2048)
        : mSize(size), mData(size)
    {}

    float* data() { return mData.data(); }
    size_t size() const { return mSize; }

    float sample(float phase) const {
        float pos = phase * mSize;
        int i0 = (int)pos;
        int i1 = (i0 + 1) % mSize;
        float frac = pos - i0;
        return mData[i0] * (1.0f - frac) + mData[i1] * frac;
    }

private:
    size_t mSize;
    std::vector<float> mData;
};

class WavetableOscillator : public Generator {
public:
    WavetableOscillator(Wavetable* table, float freq = 440.0f)
        : mTable(table)
    {
        mSweep.setFreq(freq);
    }

    void setFreq(float f) { mSweep.setFreq(f); }
    void setPhase(float p) { mSweep.setPhase(p); }

    float process() override {
        float p = mSweep.process();
        return mTable->sample(p);
    }

    float processFM(float fm) {
        float p = mSweep.processFM(fm);
        return mTable->sample(p);
    }

private:
    Sweep* mSweepPtr = nullptr;
    Sweep  mSweep;
    Wavetable* mTable;
};

/*
Usage:

Wavetable table(2048);

for (size_t i = 0; i < table.size(); ++i) {
    float p = (float)i / table.size();
    table.data()[i] = std::sin(2.0f * M_PI * p);
}
*/