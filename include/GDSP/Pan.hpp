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

class Pan {
public:
    Pan(float pos = 0.0f)
    :   
    mPan(pos)
    {
    }

    void setPos(float pos)
    {
        float mPos = std::clamp(pos, -1.f, 1.f);
        mPan.pos(mPos);
    }

    void setPosGain(float pos, float gain)
    {
        float mPos = std::clamp(pos, -1.f, 1.f);
        mPan.pos(mPos, std::max(0.f, gain));
    }

    void setPosLinear(float pos)
    {
        float mPos = std::clamp(pos, -1.f, 1.f);
        mPan.posL(mPos);
    }

    void process(float input, float& left, float& right)
    {
        mPan(input, left, right);
    }

    void run(const float* input, float* left, float* right, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            process(input[i], left[i], right[i]);
    }

private:
    gam::Pan<double> mPan;
};


