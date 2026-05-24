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

class RingMod {
public:
    float depth = 1.0f;

    float process(float carrier, float modulator)
    {
        return carrier * modulator * depth;
    }

    void run(const float* c, const float* m, float* y, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            y[i] = process(c[i], m[i]);
    }
};
