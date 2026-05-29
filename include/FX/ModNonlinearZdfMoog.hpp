#pragma once
#include <cmath>
#include <cstring>
#include <algorithm>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

// -------------------------------------------------------------
// Modulated Nonlinear ZDF Moog Ladder
// Cutoff, Resonance, Drive fully modulatable
// -------------------------------------------------------------
class ModNonlinearZdfMoog : public Function
{
public:
    explicit ModNonlinearZdfMoog(float initFreq = 1000.f,
                                 float initRes  = 0.1f,
                                 float initDrive = 1.0f)
    {
        baseCutoff = initFreq;
        baseRes    = initRes;
        baseDrive  = initDrive;
        setAM(1.0f);
        reset();
    }

    void setDrive(float d) { baseDrive = std::max(0.0f, d); }

    void reset()
    {
        std::memset(z, 0, sizeof(z));
        uPrev = 0.0;
        k = 0.0;
        G = 0.0;
        oneMinusG = 1.0;
    }

    float process(float x) override
    {
        // ---- ModFilter modulation path ----
        float mCut = cutoff.process();   // [-1,1]
        float mRes = res.process();      // [-1,1]
        float mAmp = am.process();

        float fc = baseCutoff * (1.0f + mCut);
        float r  = baseRes    * (1.0f + mRes);
        float d  = baseDrive; // (you can modulate drive too if desired)

        const float sr = (float)gam::sampleRate();
        fc = std::clamp(fc, 5.0f, 0.49f * sr);
        r  = std::clamp(r, 0.0f, 1.0f);

        updateCoeffs(fc, r, d);

        // ---- Nonlinear ZDF Solve ----
        double u = uPrev;

        constexpr int iters = 2;
        for(int it = 0; it < iters; ++it)
        {
            const double s1 = std::tanh(driveD * u);
            const double s1p = driveD * (1.0 - s1*s1);
            const double y1 = G * s1 + oneMinusG * z[0];
            const double y1p = G * s1p;

            const double s2 = std::tanh(driveD * y1);
            const double s2p = driveD * (1.0 - s2*s2);
            const double y2 = G * s2 + oneMinusG * z[1];
            const double y2p = G * s2p * y1p;

            const double s3 = std::tanh(driveD * y2);
            const double s3p = driveD * (1.0 - s3*s3);
            const double y3 = G * s3 + oneMinusG * z[2];
            const double y3p = G * s3p * y2p;

            const double s4 = std::tanh(driveD * y3);
            const double s4p = driveD * (1.0 - s4*s4);
            const double y4 = G * s4 + oneMinusG * z[3];
            const double y4p = G * s4p * y3p;

            const double fb = std::tanh(driveD * y4);
            const double fbp = driveD * (1.0 - fb*fb);

            const double f  = (u - (double)x) + k * fb;
            const double fp = 1.0 + k * fbp * y4p;

            u -= f / fp;
        }

        const double y1 = G * std::tanh(driveD * u) + oneMinusG * z[0];
        const double y2 = G * std::tanh(driveD * y1) + oneMinusG * z[1];
        const double y3 = G * std::tanh(driveD * y2) + oneMinusG * z[2];
        const double y4 = G * std::tanh(driveD * y3) + oneMinusG * z[3];

        z[0] = 2.0 * y1 - z[0];
        z[1] = 2.0 * y2 - z[1];
        z[2] = 2.0 * y3 - z[2];
        z[3] = 2.0 * y4 - z[3];

        uPrev = u;
        return (float)y4 * mAmp;
    }

private:
    void updateCoeffs(float fc, float res01, float drive)
    {
        const double sr = gam::sampleRate();
        const double g  = std::tan(M_PI * fc / sr);

        G = g / (1.0 + g);
        oneMinusG = 1.0 - G;

        k = 4.0 * res01;
        driveD = std::max(1e-6, (double)drive);
    }

private:
    double z[4]{};

    double k = 0.0;
    double G = 0.0;
    double oneMinusG = 1.0;
    double driveD = 1.0;

    double uPrev = 0.0;

    float baseDrive = 1.0f;
};
