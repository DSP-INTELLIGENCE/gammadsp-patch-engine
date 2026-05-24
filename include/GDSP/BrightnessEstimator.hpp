#pragma once
#include <algorithm>
#include <cmath>

#include "GammaDSP.hpp"
#include "GDSP_Analysis.hpp"


class BrightnessEstimator {
public:
    struct State {
        float brightness = (float)0;  // 0..1
        float noisiness  = (float)0;  // 0..1
        bool   tonal      = false;
    };

    BrightnessEstimator()
    : zcr_((float)0.02),
      peak_((float)0.02),
      tonalThresh_((float)0.25, (float)8),
      controlClock_((unsigned)((float)gam::sampleRate() * (float)0.02))
    {}

    void setWindow(float seconds)
    {
        unsigned w = (unsigned)(seconds * (float)gam::sampleRate());
        zcr_.setWindow(w);
        peak_.setWindow(w);
    }

    void setSensitivity(float s)
    {
        sensitivity_ = std::clamp(s, (float)0, (float)1);
        float t = (float)0.15 + ((float)1 - sensitivity_) * (float)0.3;
        tonalThresh_.setThreshold(t);
    }

    void setSmoothing(float hz)
    {
        smoothCoeff_ = std::clamp(hz / (float)gam::sampleRate(), (float)0, (float)1);
    }

    // audio thread
    void processfloat(float x)
    {
        zcr_.process(x);
        peak_.process(x);
    }

    // control thread
    void update(const typename LevelActivityModule<float>::State& levelState)
    {
        if (!controlClock_.process())
            return;

        float z = zcr_.value();     // 0..1
        float p = peak_.value();    // amplitude context

        float raw = z * ((float)0.5 + (float)0.5 * p);

        state_.brightness += smoothCoeff_ * (raw - state_.brightness);
        state_.noisiness = z;

        state_.tonal = tonalThresh_.process(state_.brightness) < (float)0.5;
    }

    const State& state() const { return state_; }

    void reset()
    {
        zcr_.reset();
        peak_.reset();
        tonalThresh_.reset((float)0);
        controlClock_.reset();
        state_ = {};
    }

private:
    // --- primitives ---
    ZeroCrossRate<float> zcr_;
    MaxAbs<float>       peak_;
    Threshold<float>    tonalThresh_;
    PCounter<float>     controlClock_;

    // --- parameters ---
    float sensitivity_{(float)0.5};
    float smoothCoeff_{(float)0.1};

    // --- state ---
    State state_;
};
