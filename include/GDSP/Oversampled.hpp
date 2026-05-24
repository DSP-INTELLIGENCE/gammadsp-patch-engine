#pragma once
#include <vector>
#include "Engine.hpp"
#include "Parameters.hpp"
#include "R8Resampler.hpp"

class Oversampled : public Function
{
public:
    Oversampled(Function* f)
    : function(f)
    {
        double fs = gam::sampleRate();
        up   = std::make_unique<R8Resampler>(fs, fs * OS, MaxBlock);
        down = std::make_unique<R8Resampler>(fs * OS, fs, MaxBlock);

        inBuf.resize(MaxBlock);
    }

    void setShaper(Function* f) { function = f; }

    float process(float input) override
    {
        if (!function) return input;

        // ---- float -> double ----
        inBuf[0] = static_cast<double>(input);

        // ---- Upsample ----
        auto& hi = up->process(inBuf.data(), 1);

        // ---- Nonlinear processing at high rate ----
        for (double& s : hi)
            s = function->process(static_cast<float>(s));

        // ---- Downsample ----
        auto& lo = down->process(hi.data(), (int)hi.size());

        return lo.empty() ? 0.f : static_cast<float>(lo[0]);
    }

private:
    static constexpr int OS = 4;
    static constexpr int MaxBlock = 256;

    std::unique_ptr<R8Resampler> up;
    std::unique_ptr<R8Resampler> down;
    Function* function = nullptr;

    std::vector<double> inBuf;
};
