#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <atomic>
#include <algorithm>

#include "Engine.hpp"
#include "Parameters.hpp"
#include "Biquad.hpp"

class SOS : public Function {
public:
    SOS(unsigned sections)
    {
        mSections.reserve(sections);
        for (unsigned i = 0; i < sections; ++i)
            mSections.emplace_back((float)gam::sampleRate());
    }

    void setFreq(float f) {
        float freq = (std::clamp(f, 5.f, 0.45f * (float)gam::sampleRate()));
        for (auto& sec : mSections) {
            sec.setFreq(freq);
        }
    }

    void setRes(float r) {
        float f = std::clamp(r, 0.001f, 50.f);
        for (auto& sec : mSections) {
            sec.setRes(f);
        }    
    }

    void setLevel(float l) {
        float f = std::max(0.f, l);
        for (auto& sec : mSections) {
            sec.setLevel(f);
        }
    }

    void reset()
    {
        for(size_t i = 0; i < mSections.size(); i++) mSections[i].reset();
    }
    float process(float input) override
    {    
        float x = input;
        for (auto& sec : mSections) {
            x = sec.process(x);
        }
        return x;
    }
    void run(const float* in, float* out, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }
    void setSection(size_t i, float b0, float b1, float b2, float a1, float a2)
    {
        mSections[i].setCoef(b0,b1,b2,a1,a2);
    }

    std::vector<Biquad> mSections;    
};
