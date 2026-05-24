#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif
#include <vector>
#include <algorithm>
#include <random>
#include <cstddef>
#include <cmath>

#include "Engine.hpp"
#include "Parameters.hpp"

class AllPass2Block : public Function {
public:
    AllPass2Block(float initFreq = 1000.0f, float initWidth = 100.0f, int N = 4)
    : mBaseFreq(initFreq), mBaseWidth(initWidth), mDetune(0.0f)
    {
        const int stages = std::max(1, N);

        // Phase-90 style spread
        const float baseSpread = 0.07f; // ±7%
        std::mt19937 rng(1337);
        std::uniform_real_distribution<float> tolF(0.99f, 1.01f);
        std::uniform_real_distribution<float> tolW(0.98f, 1.02f);

        mRatios.resize(stages);
        mWidthRatios.resize(stages);

        for (int i = 0; i < stages; ++i)
        {
            float pos = (float(i) / (stages - 1)) - 0.5f;
            float spread = std::pow(1.0f + baseSpread, pos * 2.0f);

            mRatios[i]      = spread * tolF(rng);
            mWidthRatios[i] = tolW(rng);

            mFilters.emplace_back(initFreq, initWidth);
        }

        updateStages();
    }

    // ---------------- Controls ----------------

    void freq(float freqHz)
    {
        float nyq = 0.49f * gam::sampleRate();
        mBaseFreq = std::clamp(freqHz, 1.f, nyq);
        updateStages();
    }

    float freq() const { return mBaseFreq; }

    void width(float widthHz)
    {
        float nyq = 0.49f * gam::sampleRate();
        mBaseWidth = std::clamp(widthHz, 0.f, nyq);
        updateStages();
    }

    float width() const { return mBaseWidth; }

    void detune(float d) // 0…1
    {
        mDetune = std::clamp(d, 0.f, 1.f);
        updateStages();
    }

    float detune() const { return mDetune; }

    void reset()
    {
        for (auto& f : mFilters)
            f.reset();
    }

    // ---------------- DSP ----------------

    float process(float input) override
    {
        float r = mFilters[0](input);
        for (size_t i = 1; i < mFilters.size(); ++i)
            r = mFilters[i](r);
        return r;
    }

    void run(const float* input, float* output, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    void updateStages()
    {
        float nyq = 0.49f * gam::sampleRate();

        for (size_t i = 0; i < mFilters.size(); ++i)
        {
            float fr = 1.0f + mDetune * (mRatios[i] - 1.0f);
            float wr = 1.0f + mDetune * (mWidthRatios[i] - 1.0f);

            float f = std::clamp(mBaseFreq * fr, 1.f, nyq);
            float w = std::clamp(mBaseWidth * wr, 0.f, nyq);

            mFilters[i].freq(f);
            mFilters[i].width(w);
        }
    }

private:
    std::vector<gam::AllPass2<double>> mFilters;
    std::vector<float> mRatios;
    std::vector<float> mWidthRatios;

    float mBaseFreq  = 1000.0f;
    float mBaseWidth = 100.0f;
    float mDetune    = 0.0f;
};

