#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif
#include <vector>

class PitchShifter {
public:
    PitchShifter(size_t winSize, size_t hopSize)
    : stft_(winSize, hopSize, 0, gam::HANN, gam::MAG_FREQ),
      yin_(winSize),
      winSize_(winSize),
      hopSize_(hopSize)
    {
        timeBuffer_.resize(winSize_, 0.f);
        mag_.resize(stft_.numBins(), 0.f);
        frq_.resize(stft_.numBins(), 0.f);
    }

    void setTarget(double hz) { targetHz_ = hz; }
    void setEnabled(bool v)   { enabled_ = v; }

    void run(float* input, float* output, size_t size)
    {
        for (size_t i = 0; i < size; ++i) {

            // maintain analysis window
            std::move(timeBuffer_.begin() + 1, timeBuffer_.end(), timeBuffer_.begin());
            timeBuffer_.back() = input[i];

            // feed STFT
            if (stft_(input[i])) {
                processFrame();
            }

            // synthesize
            output[i] = stft_();
        }
    }

private:
    void processFrame()
    {
        if (!enabled_) return;

        // --- Pitch detection ---
        PDResult r = yin_.analyze(timeBuffer_.data());
        if (r.probability < 0.6 || r.pitchHz <= 0.0) return;

        const double ratio = targetHz_ / r.pitchHz;

        // --- True PV pitch shifting in MAG_FREQ ---
        auto* bins = stft_.bins();
        const unsigned nb = stft_.numBins();

        std::fill(mag_.begin(), mag_.end(), 0.f);
        std::fill(frq_.begin(), frq_.end(), 0.f);

        for (unsigned k = 0; k < nb; ++k) {
            const float m = bins[k][0];
            const float f = bins[k][1];

            const double kd = k * ratio;
            const int k0 = int(kd);
            const int k1 = k0 + 1;
            const float w1 = kd - k0;
            const float w0 = 1.f - w1;

            if (k0 < nb) {
                mag_[k0] += m * w0;
                frq_[k0]  = f * ratio;
            }
            if (k1 < nb) {
                mag_[k1] += m * w1;
                frq_[k1]  = f * ratio;
            }
        }

        for (unsigned k = 0; k < nb; ++k) {
            bins[k][0] = mag_[k];
            bins[k][1] = frq_[k];
        }

        stft_.zeroEnds();
    }

private:
    gam::STFT stft_;
    PitchDetector yin_;

    std::vector<float> timeBuffer_;
    std::vector<float> mag_, frq_;

    size_t winSize_, hopSize_;

    double targetHz_ = 440.0;
    bool enabled_ = true;
};
