#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif
#include <cassert>
#include <vector>
#include <memory>
#include <atomic>
#include "Engine.hpp"
#include "Parameters.hpp"


class AM {
public:
    AM(float depth = 1.0f)
    : mAM(depth)
    {
    }

    void setDepth(float depth)
    {    
        mAM.depth(std::max(0.f, depth));
    }

    float process(float carrier, float modulator)
    {
        return mAM(carrier, modulator);
    }

    void run(const float* carrier,
                const float* modulator,
                float* output,
                size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(carrier[i], modulator[i]);
    }


    float mDepth = 1.0f;

private:
    gam::AM<double> mAM;
};

