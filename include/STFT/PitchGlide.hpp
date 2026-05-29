#pragma once
#include <cmath>    // std::log2, std::pow, std::floor
#include <cfloat>   // (not strictly required here, but often used for numeric limits)

class PitchGlide {
public:
    // time in seconds for full transition
    PitchGlide(double glideTime = 0.05)
        : glideTime_(glideTime) {}

    void setGlideTime(double seconds) {
        glideTime_ = std::max(0.0, seconds);
    }

    // call once per frame
    double process(double targetHz, double dt)
    {
        if (currentHz_ <= 0.0)
            currentHz_ = targetHz;

        if (glideTime_ <= 0.0)
            return currentHz_ = targetHz;

        const double alpha = dt / glideTime_;
        const double k = std::min(1.0, alpha);

        currentHz_ += (targetHz - currentHz_) * k;
        return currentHz_;
    }

    double current() const { return currentHz_; }

private:
    double glideTime_;
    double currentHz_ = 0.0;
};