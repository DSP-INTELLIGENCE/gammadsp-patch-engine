#include "VoiceActivityDetector.hpp"
#include <algorithm>

VoiceActivityDetector::VoiceActivityDetector(float sr)
: speechThresh_(0.5f, 6.0f),
  hangTimer_(unsigned(sr * 0.2f)),       // 200 ms hang time
  controlClock_(unsigned(sr * 0.02f)),   // 20 ms update
  sampleRate_(sr)
{
}

void VoiceActivityDetector::setSensitivity(float s)
{
    sensitivity_ = std::clamp(s, 0.0f, 1.0f);

    float t = 0.3f + (1.0f - sensitivity_) * 0.4f;
    speechThresh_.setThreshold(t);
}

void VoiceActivityDetector::setHangTime(float seconds)
{
    hangTimer_.setCount(unsigned(seconds * sampleRate_));
}

void VoiceActivityDetector::processSample(float) {}

void VoiceActivityDetector::update(const LevelActivityModule::State& level,
                                   const PitchFollower::State& pitch,
                                   const BrightnessEstimator::State& bright)
{
    if(!controlClock_.process())
        return;

    // Composite speech score
    float score = 0.0f;

    if(level.active)       score += 0.4f;
    if(pitch.voiced)      score += 0.4f;
    if(!bright.tonal)     score += 0.2f;

    state_.confidence = score;

    bool speechCandidate = speechThresh_.process(score) > 0.5f;

    if(speechCandidate)
    {
        state_.speaking = true;
        hangTimer_.reset();
    }
    else
    {
        bool done = hangTimer_.process(0.0f, 0.001f);
        if(done)
            state_.speaking = false;
    }
}

void VoiceActivityDetector::reset()
{
    speechThresh_.reset(0.0f);
    hangTimer_.reset();
    controlClock_.reset();
    state_ = {};
}
