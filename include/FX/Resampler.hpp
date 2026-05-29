// Resampler.hpp
#pragma once
#include <samplerate.h>
#include <vector>
#include <stdexcept>

class Resampler
{
public:
    enum class Quality {
        Best   = SRC_SINC_BEST_QUALITY,
        Medium = SRC_SINC_MEDIUM_QUALITY,
        Fast   = SRC_SINC_FASTEST
    };

    Resampler(int channels, double ratio, Quality quality = Quality::Best)
        : channels_(channels), ratio_(ratio)
    {
        int error = 0;
        state_ = src_new(static_cast<int>(quality), channels, &error);
        if (!state_)
            throw std::runtime_error(src_strerror(error));
    }

    ~Resampler()
    {
        src_delete(state_);
    }

    void setRatio(double newRatio)
    {
        ratio_ = newRatio;
        src_set_ratio(state_, ratio_);
    }

    std::vector<float> process(const float* input, size_t frames, bool endOfInput = false)
    {
        SRC_DATA data{};
        data.data_in = input;
        data.input_frames = frames;

        const size_t outFrames = static_cast<size_t>(frames * ratio_) + 64;
        output_.resize(outFrames * channels_);

        data.data_out = output_.data();
        data.output_frames = outFrames;
        data.end_of_input = endOfInput;
        data.src_ratio = ratio_;

        int err = src_process(state_, &data);
        if (err)
            throw std::runtime_error(src_strerror(err));

        output_.resize(data.output_frames_gen * channels_);
        return output_;
    }

private:
    SRC_STATE* state_ = nullptr;
    int channels_;
    double ratio_;
    std::vector<float> output_;
};
