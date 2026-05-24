// NonlinearZdfMoog.hpp
#pragma once

#include <cmath>
#include <cstring>
#include <algorithm>

// -----------------------------------------------------------------------------
// Nonlinear ZDF/TPT Moog Ladder (Zavalishin-style)
// - tanh nonlinearity in each stage + tanh in feedback path
// - Newton-Raphson solves the zero-delay feedback each sample
//
// Params:
//   cutoffHz   : Hz
//   resonance01: 0..1 (mapped to k = 0..4)
//   drive      : >= 0 (applied inside tanh arguments; 1.0 = neutral)
//
// Usage:
//   NonlinearZdfMoog f(sr);
//   f.setCutoff(1000);
//   f.setResonance(0.7f);
//   f.setDrive(1.0f);
//   y = f.process(x);
// -----------------------------------------------------------------------------
class NonlinearZdfMoog
{
public:
    explicit NonlinearZdfMoog(float sr) : sampleRate(sr)
    {
        reset();
        setCutoff(1000.0f);
        setResonance(0.1f);
        setDrive(1.0f);
    }

    void reset()
    {
        std::memset(z, 0, sizeof(z));
        uPrev = 0.0;
        cutoff = 1000.0f;
        resonance = 0.0f;
        drive = 1.0f;

        k = 0.0;
        G = 0.0;
        oneMinusG = 1.0;
    }

    inline float process(float x)
    {
        // Newton initial guess: previous solution (fast convergence)
        double u = uPrev;

        // 1–2 iterations is typical. Use 2 for robustness at high resonance/drive.
        // You can make this runtime-configurable if desired.
        constexpr int iters = 2;

        for (int it = 0; it < iters; ++it)
        {
            // Forward propagate ladder for this trial u, while tracking dy4/du.

            // Stage 1
            const double s1_in = u;
            const double s1 = std::tanh(driveD * s1_in);
            const double s1p = driveD * (1.0 - s1 * s1);          // d/d(u) tanh(d*u)
            const double y1 = G * s1 + oneMinusG * z[0];
            const double y1p = G * s1p;                           // dy1/du

            // Stage 2
            const double s2_in = y1;
            const double s2 = std::tanh(driveD * s2_in);
            const double s2p = driveD * (1.0 - s2 * s2);          // d/d(y1) tanh(d*y1)
            const double y2 = G * s2 + oneMinusG * z[1];
            const double y2p = G * s2p * y1p;                     // dy2/du

            // Stage 3
            const double s3_in = y2;
            const double s3 = std::tanh(driveD * s3_in);
            const double s3p = driveD * (1.0 - s3 * s3);
            const double y3 = G * s3 + oneMinusG * z[2];
            const double y3p = G * s3p * y2p;

            // Stage 4
            const double s4_in = y3;
            const double s4 = std::tanh(driveD * s4_in);
            const double s4p = driveD * (1.0 - s4 * s4);
            const double y4 = G * s4 + oneMinusG * z[3];
            const double y4p = G * s4p * y3p;                     // dy4/du

            // Feedback nonlinearity: u = x - k * tanh(d * y4)
            const double fb = std::tanh(driveD * y4);
            const double fbp = driveD * (1.0 - fb * fb);          // d/d(y4) tanh(d*y4)

            // f(u) = u - x + k*fb
            const double f = (u - (double)x) + k * fb;

            // f'(u) = 1 + k * fbp * dy4/du
            const double fp = 1.0 + k * fbp * y4p;

            // Newton step (guard against pathological fp ~ 0)
            const double du = f / fp;
            u -= du;

            // Optional early exit if converged
            if (std::abs(du) < 1e-12)
                break;
        }

        // With final u, compute final ladder output y4 and update states.
        // (We recompute forward pass once; keeps code simple and consistent.)
        const double y1 = G * std::tanh(driveD * u) + oneMinusG * z[0];
        const double y2 = G * std::tanh(driveD * y1) + oneMinusG * z[1];
        const double y3 = G * std::tanh(driveD * y2) + oneMinusG * z[2];
        const double y4 = G * std::tanh(driveD * y3) + oneMinusG * z[3];

        // TPT state updates: z <- 2y - z
        z[0] = 2.0 * y1 - z[0];
        z[1] = 2.0 * y2 - z[1];
        z[2] = 2.0 * y3 - z[2];
        z[3] = 2.0 * y4 - z[3];

        uPrev = u;
        return (float)y4;
    }

    void setResonance(float r)
    {
        resonance = std::max(0.0f, r);
        const double r01 = std::min(1.0, (double)resonance);

        // Standard mapping: k in [0,4]
        k = 4.0 * r01;
    }

    void setCutoff(float hz)
    {
        const double nyq = 0.5 * sampleRate;
        const double fc = std::clamp((double)hz, 1.0, 0.49 * nyq);
        cutoff = (float)fc;

        // Prewarp then convert to TPT coefficient
        const double g = std::tan(M_PI * fc / sampleRate);
        G = g / (1.0 + g);
        oneMinusG = 1.0 - G;
    }

    void setDrive(float d)
    {
        // keep drive >= 0; drive=0 turns tanh into tanh(0)=0 (silence),
        // so clamp to a small positive minimum if you prefer.
        drive = std::max(0.0f, d);
        driveD = (double)drive;
    }

private:
    double z[4]{};

    float sampleRate;
    float cutoff = 1000.0f;
    float resonance = 0.0f;
    float drive = 1.0f;

    double driveD = 1.0;
    double k = 0.0;
    double G = 0.0;
    double oneMinusG = 1.0;

    double uPrev = 0.0;
};
