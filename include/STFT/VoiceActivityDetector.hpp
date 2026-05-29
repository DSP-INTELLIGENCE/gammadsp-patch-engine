#pragma once

#include "GammaDSP.hpp"
#include "GDSP_Analysis.hpp"

class VoiceActivityDetector {
public:
    struct State {
        bool  speaking = false;
        float confidence = 0.0f;   // 0..1
    };

    VoiceActivityDetector(float sampleRate);

    void setSensitivity(float s);   // 0..1
    void setHangTime(float seconds);

    // audio thread
    void processSample(float x);

    // control thread
    void update(const LevelActivityModule::State& level,
                const PitchFollower::State& pitch,
                const BrightnessEstimator::State& bright);

    const State& state() const { return state_; }

    void reset();

private:
    // --- primitives ---
    Threshold    speechThresh_;
    SilenceDetect hangTimer_;
    PCounter     controlClock_;

    // --- parameters ---
    float sampleRate_;
    float sensitivity_{0.5f};

    // --- state ---
    State state_;
};
