#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif
#include <vector>
#include <algorithm>
#include <random>

#include "Engine.hpp"
#include "Parameters.hpp"

class AllPass1Block : public Function {
public:
    AllPass1Block(float initFreq = 1000.0f, int N = 4)
    : mBaseFreq(initFreq), mDetune(0.0f)
    {
        const int stages = std::max(1, N);

        // Phase-90 style geometric spread
        const float baseSpread = 0.07f;  // ±7%
        std::mt19937 rng(1337);
        std::uniform_real_distribution<float> tol(0.99f, 1.01f);

        mRatios.resize(stages);

        for (int i = 0; i < stages; ++i)
        {
            float pos = (float(i) / (stages - 1)) - 0.5f;
            float spread = std::pow(1.0f + baseSpread, pos * 2.0f);
            float jitter = tol(rng);

            mRatios[i] = spread * jitter;
            mFilters.emplace_back(initFreq);
        }

        freq(initFreq);
    }

    // ---------------- Controls ----------------

    void freq(float freqHz)
    {
        mBaseFreq = std::max(1.f, freqHz);
        updateStages();
    }

    float freq() const { return mBaseFreq; }

    void setDetune(float d)   // 0…1
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
            float ratio = 1.0f + mDetune * (mRatios[i] - 1.0f);
            float f = std::clamp(mBaseFreq * ratio, 1.f, nyq);
            mFilters[i].freq(f);
        }
    }

private:
    std::vector<gam::AllPass1<double>> mFilters;
    std::vector<float> mRatios;

    float mBaseFreq = 1000.0f;
    float mDetune   = 0.0f;
};
