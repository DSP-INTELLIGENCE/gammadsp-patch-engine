#pragma once
#include "r8bbase.h"
#include "CDSPResampler.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <cstring>

class R8Resampler
{
public:
    R8Resampler(double inRate, double outRate, int maxInFrames)
    : maxIn_(maxInFrames)
    {
        rs_ = std::make_unique<r8b::CDSPResampler>(inRate, outRate, maxInFrames);

        // Worst-case output frames (very conservative).
        // r8brain can output slightly more than ratio * in due to filter latency management.
        // Use a generous cap to never resize in audio thread.
        const double ratio = outRate / inRate;
        maxOut_ = int(std::ceil(maxInFrames * ratio)) + 64;

        outBuf_.resize(maxOut_);
        tmpIn_.resize(maxIn_); // for float->double staging if needed
    }

    // If you already have doubles:
    inline const double* process(const double* in, int inFrames, int& outFrames)
    {
        double* out = nullptr;
        // r8brain API is NOT const-correct; it expects double*.
        // We only pass a non-const pointer when we truly own the memory.
        outFrames = rs_->process(const_cast<double*>(in), inFrames, out);

        // Copy to our stable buffer (out pointer invalidates next call)
        const int n = std::min(outFrames, maxOut_);
        std::copy_n(out, n, outBuf_.data());
        outFrames = n;
        return outBuf_.data();
    }

    inline const double* flush(int& outFrames)
    {
        double* out = nullptr;
        outFrames = rs_->process(nullptr, 0, out);

        const int n = std::min(outFrames, maxOut_);
        std::copy_n(out, n, outBuf_.data());
        outFrames = n;
        return outBuf_.data();
    }

    // Convenience: process float input without allocations
    inline const double* processFloat(const float* in, int inFrames, int& outFrames)
    {
        const int n = std::min(inFrames, maxIn_);
        for (int i = 0; i < n; ++i) tmpIn_[i] = (double)in[i];
        return process(tmpIn_.data(), n, outFrames);
    }

    int maxIn()  const { return maxIn_; }
    int maxOut() const { return maxOut_; }

private:
    std::unique_ptr<r8b::CDSPResampler> rs_;

    int maxIn_  = 0;
    int maxOut_ = 0;

    std::vector<double> outBuf_; // stable output for caller
    std::vector<double> tmpIn_;  // staging for float input
};
