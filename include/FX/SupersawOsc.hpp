#pragma once
#include <vector>
#include <cmath>
#include "WavetableOsc.hpp"

class SupersawOsc : public ModGenerator {
public:
    SupersawOsc(float freq = 110.f, unsigned voices = 7)
    : mBase(freq), mVoices(voices)
    {
        setVoices(voices);
    }

    void loadSerumFile(const std::string& path) {
        for(auto& v : mOscs)
            v.loadSerumFile(path);
    }

    void setPosition(float p) {
        for(auto& v : mOscs)
            v.setPosition(p);
    }

    void setVoices(unsigned v) {
        mVoices = std::max(1u, v);
        rebuild();
    }

    void setDetune(float d) { mDetune = d; rebuild(); }
    void setWidth(float w)  { mWidth = w; }

    float process() override {
        float out = 0.f;

        for(size_t i=0;i<mOscs.size();++i) {
            float voice = update(mOscs[i]);
            out += voice * mGain[i];
        }

        return out;
    }

private:
    void rebuild() {
        mOscs.clear();
        mGain.clear();

        for(unsigned i=0;i<mVoices;i++) {
            float x = (float(i) / (mVoices - 1) - 0.5f) * 2.f;
            float det = std::pow(2.f, (x * x * x * mDetune) / 1200.f);

            WavetableOsc osc(mBase);
            osc.setFreq(mBase * det);

            // amplitude shaping curve (JP-style)
            float amp = 1.f - std::abs(x);
            amp = amp * amp;

            mOscs.push_back(osc);
            mGain.push_back(amp);
        }
    }

private:
    float mBase;
    unsigned mVoices;
    float mDetune = 20.f;     // cents
    float mWidth  = 0.8f;

    std::vector<WavetableOsc> mOscs;
    std::vector<float> mGain;
};
