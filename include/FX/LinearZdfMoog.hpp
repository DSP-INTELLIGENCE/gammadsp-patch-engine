#pragma once

#include <cmath>
#include <cstring>
#include <algorithm>

// -----------------------------------------------------------------------------
// Linear Zavalishin TPT / ZDF Moog Ladder (4-pole)
// • Linear (no nonlinearities)
// • True zero-delay feedback
// • Stable self-oscillation
// • Cutoff in Hz, resonance in [0..1]
//
// Usage:
//   LinearZdfMoog moog(sr);
//   moog.setCutoff(1000);
//   moog.setResonance(0.7f);
//   y = moog.process(x);
// -----------------------------------------------------------------------------
class LinearZdfMoog
{
public:
    explicit LinearZdfMoog(float sr) : sampleRate(sr)
    {
        reset();
        setCutoff(1000.0f);
        setResonance(0.1f);
    }

    void reset()
    {
        std::memset(z, 0, sizeof(z));
        cutoff = 1000.0f;
        resonance = 0.0f;
        k = 0.0;
        G = G2 = G3 = G4 = 0.0;
        oneMinusG = 1.0;
        denom = 1.0;
    }

    // One sample in → one sample out
    inline float process(float x)
    {
        // Compute affine term b: y4 = a*u + b  with a = G^4
        const double b =
            oneMinusG * (G3 * z[0] + G2 * z[1] + G * z[2] + z[3]);

        // Closed-form ZDF solve for feedback
        const double u = (x - k * b) / denom;

        // 4 cascaded TPT one-poles
        double y1 = G * u  + oneMinusG * z[0];
        double y2 = G * y1 + oneMinusG * z[1];
        double y3 = G * y2 + oneMinusG * z[2];
        double y4 = G * y3 + oneMinusG * z[3];

        // Update integrators
        z[0] = 2.0 * y1 - z[0];
        z[1] = 2.0 * y2 - z[1];
        z[2] = 2.0 * y3 - z[2];
        z[3] = 2.0 * y4 - z[3];

        return (float)y4;
    }

    void setResonance(float r)
    {
        resonance = std::max(0.0f, r);
        double r01 = std::min(1.0, (double)resonance);

        // Moog self-oscillates near k ≈ 4
        k = 4.0 * r01;

        // ZDF denominator
        denom = 1.0 + k * G4;
    }

    void setCutoff(float hz)
    {
        const double nyq = 0.5 * sampleRate;
        double fc = std::clamp((double)hz, 1.0, 0.49 * nyq);

        cutoff = (float)fc;

        // Zavalishin prewarp
        const double g = std::tan(M_PI * fc / sampleRate);

        // TPT coefficient
        G = g / (1.0 + g);

        G2 = G * G;
        G3 = G2 * G;
        G4 = G2 * G2;

        oneMinusG = 1.0 - G;

        // Update feedback denominator
        denom = 1.0 + k * G4;
    }

private:
    double z[4]{};

    float  sampleRate;
    float  cutoff = 1000.0f;
    float  resonance = 0.0f;

    double k = 0.0;
    double G = 0.0, G2 = 0.0, G3 = 0.0, G4 = 0.0;
    double oneMinusG = 1.0;
    double denom = 1.0;
};
