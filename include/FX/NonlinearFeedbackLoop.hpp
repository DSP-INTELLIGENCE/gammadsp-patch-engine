#pragma once
#include "FeedbackShaper.hpp"
#include "ResonanceLimiter.hpp"
#include "SelfOscillationController.hpp"

struct NonlinearFeedbackLoop
{
    FeedbackShaper shaper;
    ResonanceLimiter limiter;
    SelfOscillationController osc;

    float feedbackGain = 1.f;

    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    inline float process(float feedback, float input)
    {
        // 1. Nonlinear feedback shaping
        float fb = shaper.process(feedback * feedbackGain);

        // 2. Resonance safety
        fb = limiter.process(fb);

        // 3. Oscillation control
        fb = osc.process(fb, fabsf(input));

        return zap(fb);
    }
};
