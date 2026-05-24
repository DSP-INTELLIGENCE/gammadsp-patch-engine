#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

#include "GammaDSP.hpp"
#include "GDSP_Analysis.hpp"

template <class Sample>
class PitchFollower {
public:
    struct State {
        Sample pitchHz    = (Sample)0;
        Sample confidence = (Sample)0;
        bool   voiced     = false;
    };

    explicit PitchFollower(std::size_t yinBufferSize = 2048)
    : yin_(yinBufferSize),
      env_((Sample)30),
      voicedThresh_((Sample)0.02, (Sample)10),
      controlClock_((unsigned)((Sample)gam::sampleRate() * (Sample)0.02)),
      frame_(yinBufferSize, (Sample)0)
    {}

    void setMinFrequency(Sample hz) { minHz_ = hz; }
    void setMaxFrequency(Sample hz) { maxHz_ = hz; }

    void setSensitivity(Sample s)
    {
        Sample t = (Sample)0.005 + ((Sample)1 - s) * (Sample)0.03;
        voicedThresh_.setThreshold(t);
    }

    void setSmoothing(Sample hz)
    {
        smoothCoeff_ = std::clamp(hz / (Sample)gam::sampleRate(), (Sample)0, (Sample)1);
    }

    // audio thread
    void processSample(Sample x)
    {
        env_.process(x);
        frame_[writePos_++] = x;

        if (writePos_ >= frame_.size())
            writePos_ = 0;
    }

    // control thread
    void update(const typename LevelActivityModule<Sample>::State& levelState)
    {
        if (!controlClock_.process())
            return;

        bool voiced = voicedThresh_.process(levelState.level) > (Sample)0.5;

        state_.voiced     = voiced;
        state_.confidence = (Sample)0;

        if (!voiced)
            return;

        auto result = yin_.analyze(frame_.data());
        Sample f = result.pitchHz;

        if (f <= (Sample)0 || f < minHz_ || f > maxHz_)
            return;

        // Smooth pitch
        state_.pitchHz += smoothCoeff_ * (f - state_.pitchHz);
        state_.confidence = result.probability;
    }

    const State& state() const { return state_; }

    void reset()
    {
        yin_.reset();
        env_.reset();
        voicedThresh_.reset((Sample)0);
        controlClock_.reset();

        writePos_ = 0;
        std::fill(frame_.begin(), frame_.end(), (Sample)0);

        state_ = {};
    }

private:
    // --- analysis ---
    YinPitchTracker<Sample> yin_;
    EnvFollow<Sample>       env_;
    Threshold<Sample>       voicedThresh_;
    PCounter<Sample>        controlClock_;

    // --- parameters ---
    Sample minHz_{(Sample)70};
    Sample maxHz_{(Sample)1200};

    // --- buffers ---
    std::vector<Sample> frame_;
    std::size_t writePos_{0};

    // --- smoothing ---
    Sample smoothCoeff_{(Sample)0.1};

    // --- state ---
    State state_;
};
