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

class Hilbert {
public:
    Hilbert()
    {
        
    }

    void reset()
    {
        mFilter.reset();
    }

    ComplexSample process(float input) {
        gam::Complex<double> v = mFilter(input);
        return ComplexSample(v.r,v.i);
    }
    void run(const float* input,
                    float* out_real,
                    float* out_imag,
                    size_t n)
    {
        for(size_t i = 0; i < n; ++i)
        {
            gam::Complex<double> v = mFilter(input[i]);
            out_real[i] = v.r;
            out_imag[i] = v.i;
        }
    }

private:
    gam::Hilbert<double> mFilter;
};

