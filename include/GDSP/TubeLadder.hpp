#pragma once
#include "TriodePole.hpp"
#include "NonlinearFeedbackLoop.hpp"

struct TubeLadder
{
    float sr = 44100.f;

    TriodePole s1, s2, s3, s4;

    // Nonlinear resonance feedback conditioning (your system)
    NonlinearFeedbackLoop fbLoop;

    // Controls
    float cutoff    = 1000.f;   // Hz
    float resonance = 0.2f;     // 0..~1.2
    float drive     = 1.0f;     // input drive

    float k = 0.f;             // feedback amount (≈ 4*res)
    float lastOut = 0.f;        // output cache

    void init(float fs)
    {
        sr = fs;

        s1.init(sr); s2.init(sr); s3.init(sr); s4.init(sr);
        reset();
        set(cutoff, resonance);

        // sane defaults for the loop
        fbLoop.feedbackGain = 1.0f;
        // (fbLoop.shaper / limiter / osc params should be set by caller)
    }

    void reset()
    {
        s1.reset(); s2.reset(); s3.reset(); s4.reset();
        lastOut = 0.f;
    }

    void set(float fc, float res)
    {
        cutoff    = std::clamp(fc, 5.f, 0.45f * sr);
        resonance = std::clamp(res, 0.f, 1.2f);
        k         = 4.f * resonance;

        s1.setPoleCut(cutoff);
        s2.setPoleCut(cutoff);
        s3.setPoleCut(cutoff);
        s4.setPoleCut(cutoff);
    }

    void setDrive(float d)
    {
        drive = std::max(0.f, d);
    }

    // “ZDF-ish” without Newton: 2 iterations of the feedback path
    inline float process(float x)
    {
        x *= drive;

        float u = x;
        float y4 = lastOut;

        for (int it = 0; it < 2; ++it)
        {
            // 1) shape/limit/osc-control the resonance signal from output
            float fb = fbLoop.process(y4, x);

            // 2) classic ladder sum: input minus feedback
            u = x - k * fb;

            // 3) cascade 4 triode poles
            // Optional: feed a *little* of fb into stage-1 grid for “tube loop feel”
            float y1 = s1.process(u, /*fbToGrid*/ 0.15f * k * fb);
            float y2 = s2.process(y1);
            float y3 = s3.process(y2);
            y4       = s4.process(y3);
        }

        lastOut = y4;
        return y4;
    }
};
