#pragma once
#include <vector>
#include <random>
#include <cmath>
#include "WavetableOsc.hpp"

class WavetableUnisonOsc : public WavetableOsc {
public:
    WavetableUnisonOsc(float freq = 440.f, unsigned voices = 1)
    : WavetableOsc(freq), mVoices(voices)
    {
        setUnison(voices);
    }

    // ---------- UNISON CONTROL ----------

    void setUnison(unsigned voices) {
        mVoices = std::max(1u, voices);
        rebuildVoices();
    }

    void setDetune(float d)   { mDetune = d; rebuildVoices(); }
    void setSpread(float s)   { mSpread = s; }
    void setStereo(float w)   { mStereo = w; }

    // ---------- AUDIO ----------

    float process() override {
        float sum = 0.f;

        for(size_t i = 0; i < mOscs.size(); ++i) {
            float pan = mPan[i];
            float v = WavetableOsc::update(mOscs[i]);
            sum += v * (1.f - std::abs(pan));
        }

        return sum / mOscs.size();
    }

private:
    void rebuildVoices() {
        mOscs.clear();
        mPan.clear();

        for(unsigned i = 0; i < mVoices; ++i) {
            float offset = (float(i) / (mVoices - 1) - 0.5f) * 2.f;
            float cents = offset * mDetune;
            float ratio = std::pow(2.f, cents / 1200.f);

            WavetableOsc voice(*this);
            voice.setFreq(baseFreq * ratio);

            mOscs.push_back(voice);
            mPan.push_back(offset * mStereo);
        }
    }

private:
    unsigned mVoices = 1;
    float mDetune = 10.f;   // cents
    float mSpread = 1.f;
    float mStereo = 0.7f;

    std::vector<WavetableOsc> mOscs;
    std::vector<float> mPan;
};
