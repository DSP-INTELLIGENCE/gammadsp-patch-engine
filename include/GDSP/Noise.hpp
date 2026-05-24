#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif

#include "Engine.hpp"
#include <cstdint>

//////////////////////////////////////////////////////////
// Base Noise Wrapper
//////////////////////////////////////////////////////////

class NoiseBase : public Generator {
public:
    virtual void seed(uint32_t v) = 0;
};

//////////////////////////////////////////////////////////
// White Noise
//////////////////////////////////////////////////////////

class NoiseWhite : public NoiseBase {
public:
    NoiseWhite(uint32_t seed = 0) { if(seed) mNoise.seed(seed); }

    void seed(uint32_t v) override { mNoise.seed(v); }

    float process() override {
        return mNoise();
    }

private:
    gam::NoiseWhite<> mNoise;
};

//////////////////////////////////////////////////////////
// Pink Noise
//////////////////////////////////////////////////////////

class NoisePink : public NoiseBase {
public:
    NoisePink(uint32_t seed = 0) : mNoise(seed) {}

    void seed(uint32_t v) override { mNoise.seed(v); }

    float process() override {
        return mNoise();
    }

private:
    gam::NoisePink<> mNoise;
};

//////////////////////////////////////////////////////////
// Brown Noise
//////////////////////////////////////////////////////////

class NoiseBrown : public NoiseBase {
public:
    NoiseBrown(float start = 0.0f,
               float step = 0.04f,
               float min = -1.0f,
               float max = 1.0f,
               uint32_t seed = 0)
    : mNoise(start, step, min, max, seed) {}

    void seed(uint32_t v) override { mNoise.seed(v); }

    void setStep(float v) { mNoise.step = v; }
    void setRange(float mn, float mx) { mNoise.min = mn; mNoise.max = mx; }

    float process() override {
        return mNoise();
    }

private:
    gam::NoiseBrown<> mNoise;
};

//////////////////////////////////////////////////////////
// Violet Noise
//////////////////////////////////////////////////////////

class NoiseViolet : public NoiseBase {
public:
    NoiseViolet(uint32_t seed = 0) { if(seed) mNoise.seed(seed); }

    void seed(uint32_t v) override { mNoise.seed(v); }

    float process() override {
        return mNoise();
    }

private:
    gam::NoiseViolet<> mNoise;
};

//////////////////////////////////////////////////////////
// Binary Noise
//////////////////////////////////////////////////////////

class NoiseBinary : public NoiseBase {
public:
    NoiseBinary(float amp = 1.0f, uint32_t seed = 0)
    : mNoise(amp, seed) {}

    void seed(uint32_t v) override { mNoise.seed(v); }

    void setAmp(float v) { mNoise.amp = v; }

    float process() override {
        return mNoise();
    }

private:
    gam::NoiseBinary<> mNoise;
};
