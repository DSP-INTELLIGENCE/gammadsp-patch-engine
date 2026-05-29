#pragma once
#include <cmath>
#include <algorithm>
#include <cstring>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

// -------------------------------------------------------------
// Modulated ZDF Moog Ladder (4-pole, linear)
// Cutoff + Resonance fully modulatable via ModFilter
// -------------------------------------------------------------
class ZdfMoog : public Function
{
public:
    explicit ZdfMoog(float initFreq = 1000.f, float initRes = 0.1f)
    {
        baseCutoff = initFreq;
        baseRes    = initRes;
        setAM(1.0f);
        reset();
    }

    void reset()
    {
        std::memset(z, 0, sizeof(z));
        k = 0.0;
        G = G2 = G3 = G4 = 0.0;
        oneMinusG = 1.0;
        denom = 1.0;
    }

    float process(float x) override
    {
        // ---- ModFilter modulation path ----
        float mCut = cutoff.process();   // [-1,1]
        float mRes = res.process();      // [-1,1]
        float mAmp = am.process();

        float fc = baseCutoff * (1.0f + mCut);
        float r  = baseRes    * (1.0f + mRes);

        // Safety
        const float sr = (float)gam::sampleRate();
        fc = std::clamp(fc, 5.0f, 0.49f * sr);
        r  = std::clamp(r, 0.0f, 1.0f);

        updateCoeffs(fc, r);

        // ---- ZDF Ladder Core ----
        const double b =
            oneMinusG * (G3 * z[0] + G2 * z[1] + G * z[2] + z[3]);

        const double u = (x - k * b) / denom;

        double y1 = G * u  + oneMinusG * z[0];
        double y2 = G * y1 + oneMinusG * z[1];
        double y3 = G * y2 + oneMinusG * z[2];
        double y4 = G * y3 + oneMinusG * z[3];

        z[0] = 2.0 * y1 - z[0];
        z[1] = 2.0 * y2 - z[1];
        z[2] = 2.0 * y3 - z[2];
        z[3] = 2.0 * y4 - z[3];

        return (float)y4 * mAmp;
    }

private:
    void updateCoeffs(float fc, float res01)
    {
        const double sr = gam::sampleRate();
        const double g  = std::tan(M_PI * fc / sr);

        G = g / (1.0 + g);
        G2 = G * G;
        G3 = G2 * G;
        G4 = G2 * G2;

        oneMinusG = 1.0 - G;

        // resonance → feedback
        k = 4.0 * res01;

        denom = 1.0 + k * G4;
    }

private:
    double z[4]{};

    double k = 0.0;
    double G = 0.0, G2 = 0.0, G3 = 0.0, G4 = 0.0;
    double oneMinusG = 1.0;
    double denom = 1.0;
};
