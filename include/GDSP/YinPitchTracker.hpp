#pragma once
#include <cstddef>
#include <vector>
#include <algorithm>
#include <cmath>


struct YinResult {
    double pitchHz;     
    double probability;
    int    tau;
    double betterTau;
};


class YinPitchTracker {
public:

    explicit YinPitchTracker(std::size_t bufferSize,
                             double threshold = (double)0.15)
    : bufferSize_(bufferSize),
      halfBufferSize_(bufferSize / 2),
      threshold_(threshold),
      yinBuffer_(halfBufferSize_, (double)0)
    {}

    void setThreshold(double threshold) { threshold_ = threshold; }

    double getPitch(const double* buffer)
    {
        return analyze(buffer).pitchHz;
    }

    YinResult analyze(const double* buffer)
    {
        YinResult r{};
        r.pitchHz = (double)-1;

        reset();
        difference(buffer);
        cumulativeMeanNormalizedDifference();

        const int tau = absoluteThreshold();
        r.tau = tau;
        r.probability = probability_;

        if (tau != -1)
        {
            const double betterTau = parabolicInterpolation(tau);
            r.betterTau = betterTau;
            if (betterTau > (double)0)
                r.pitchHz = (double)gam::doubleRate() / betterTau;
        }

        return r;
    }

    double getProbability() const noexcept { return probability_; }

    void reset() noexcept
    {
        std::fill(yinBuffer_.begin(), yinBuffer_.end(), (double)0);
        probability_ = (double)0;
    }

private:
    void difference(const double* buffer) noexcept
    {
        for (std::size_t tau = 0; tau < halfBufferSize_; ++tau)
        {
            double sum = 0;
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
        double runningSum = 0;
        yinBuffer_[0] = (double)1;

        for (std::size_t tau = 1; tau < halfBufferSize_; ++tau)
        {
            runningSum += yinBuffer_[tau];
            if (runningSum > (double)0)
                yinBuffer_[tau] *= (double)tau / runningSum;
            else
                yinBuffer_[tau] = (double)1;
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

                probability_ = (double)1 - yinBuffer_[tau];
                return static_cast<int>(tau);
            }
        }

        probability_ = (double)0;
        return -1;
    }

    double parabolicInterpolation(int tauEstimate) const noexcept
    {
        const int x0 = (tauEstimate < 1) ? tauEstimate : tauEstimate - 1;
        const int x2 = (tauEstimate + 1 < static_cast<int>(halfBufferSize_))
                         ? tauEstimate + 1
                         : tauEstimate;

        if (x0 == tauEstimate || x2 == tauEstimate)
            return (double)tauEstimate;

        const double s0 = yinBuffer_[x0];
        const double s1 = yinBuffer_[tauEstimate];
        const double s2 = yinBuffer_[x2];

        const double denom = (double)2 * ((double)2 * s1 - s2 - s0);
        if (denom == (double)0)
            return (double)tauEstimate;

        return (double)tauEstimate + (s2 - s0) / denom;
    }

private:
    std::size_t bufferSize_{0};
    std::size_t halfBufferSize_{0};

    double threshold_{(double)0.15};
    double probability_{(double)0};

    std::vector<double> yinBuffer_;
};
