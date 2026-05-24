#pragma once
#include <cstddef>
#include <vector>
#include <algorithm>
#include <cmath>

struct PDResult {
    double pitchHz = -1.0;
    double probability = 0.0;
    int    tau = -1;
    double betterTau = 0.0;
};

class PitchDetector {
public:
    PitchDetector(std::size_t bufferSize,                  
                  double threshold = 0.15)
        : bufferSize_(bufferSize),
          halfBufferSize_(bufferSize / 2),          
          threshold_(threshold),
          yinBuffer_(halfBufferSize_, 0.0)
    {}

    void setThreshold(double threshold) { threshold_ = threshold; }

    PDResult analyze(const float* buffer)
    {
        PDResult r;

        difference(buffer);
        cumulativeMeanNormalizedDifference();

        const int tau = absoluteThreshold();
        r.tau = tau;
        r.probability = probability_;

        if (tau > 0)
        {
            r.betterTau = parabolicInterpolation(tau);
            if (r.betterTau > 0.0)
                r.pitchHz = gam::sampleRate() / r.betterTau;
        }

        return r;
    }

private:
    void difference(const float* buffer) noexcept
    {
        yinBuffer_[0] = 0.0;

        for (std::size_t tau = 1; tau < halfBufferSize_; ++tau)
        {
            double sum = 0.0;
            for (std::size_t i = 0; i < halfBufferSize_; ++i)
            {
                const double delta = buffer[i] - buffer[i + tau];
                sum += delta * delta;
            }
            yinBuffer_[tau] = sum;
        }
    }

    void cumulativeMeanNormalizedDifference() noexcept
    {
        double runningSum = 0.0;
        yinBuffer_[0] = 1.0;

        for (std::size_t tau = 1; tau < halfBufferSize_; ++tau)
        {
            runningSum += yinBuffer_[tau];
            yinBuffer_[tau] = (runningSum > 0.0)
                ? yinBuffer_[tau] * tau / runningSum
                : 1.0;
        }
    }

    int absoluteThreshold() noexcept
    {
        for (std::size_t tau = 2; tau < halfBufferSize_; ++tau)
        {
            if (yinBuffer_[tau] < threshold_)
            {
                while (tau + 1 < halfBufferSize_ &&
                       yinBuffer_[tau + 1] < yinBuffer_[tau])
                    ++tau;

                probability_ = 1.0 - yinBuffer_[tau];
                return static_cast<int>(tau);
            }
        }

        probability_ = 0.0;
        return -1;
    }

    double parabolicInterpolation(int tauEstimate) const noexcept
    {
        const int x0 = std::max(tauEstimate - 1, 0);
        const int x2 = std::min(tauEstimate + 1, int(halfBufferSize_ - 1));

        if (x0 == tauEstimate || x2 == tauEstimate)
            return double(tauEstimate);

        const double s0 = yinBuffer_[x0];
        const double s1 = yinBuffer_[tauEstimate];
        const double s2 = yinBuffer_[x2];

        const double denom = 2.0 * (2.0 * s1 - s2 - s0);
        if (denom == 0.0)
            return double(tauEstimate);

        return tauEstimate + (s2 - s0) / denom;
    }

private:
    std::size_t bufferSize_;
    std::size_t halfBufferSize_;

    
    double threshold_;
    double probability_ = 0.0;

    std::vector<double> yinBuffer_;
};
