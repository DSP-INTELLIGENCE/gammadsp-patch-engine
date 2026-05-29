#pragma once
#include "Delay.hpp"
#include "DFXCore.hpp"

class DFXJunoChorus : public DFXCore
{
public:
    DFXJunoChorus()
    : dL(0.05f, 0.012f),
      dR(0.05f, 0.013f)
    {
        dL.setIpolType(gam::ipl::Type::CUBIC);
        dR.setIpolType(gam::ipl::Type::CUBIC);

        setWet(1.0f);
        setDry(1.0f);
        setMix(0.5f);

        setMode(1);
    }

    // Mode 1 = slow, wide
    // Mode 2 = fast, deep
    void setMode(int m)
    {
        mode = (m <= 1 ? 1 : 2);

        if (mode == 1) {
            rate1 = 0.17f;
            rate2 = 0.23f;
            depth1 = 0.0035f;
            depth2 = 0.0045f;
        }
        else {
            rate1 = 0.42f;
            rate2 = 0.60f;
            depth1 = 0.0060f;
            depth2 = 0.0075f;
        }
    }

    float process(float x) override
    {
        float sr = gam::sampleRate();

        // Dual unsynced LFOs (Juno signature)
        float lfo1 = sinf(p1);
        float lfo2 = sinf(p2);

        p1 += 2.f * float(M_PI) * rate1 / sr;
        p2 += 2.f * float(M_PI) * rate2 / sr;
        if (p1 > 2.f * float(M_PI)) p1 -= 2.f * float(M_PI);
        if (p2 > 2.f * float(M_PI)) p2 -= 2.f * float(M_PI);

        // Fixed base delays
        float tL = 0.012f + depth1 * lfo1 + depth2 * lfo2;
        float tR = 0.013f - depth1 * lfo1 - depth2 * lfo2;

        tL = std::clamp(tL, 0.0001f, 0.045f);
        tR = std::clamp(tR, 0.0001f, 0.045f);

        dL.setDelay(tL);
        dR.setDelay(tR);

        float wL = dL.process(x);
        float wR = dR.process(x);

        // Juno stereo projection
        float L = wL + 0.7f * wR;
        float R = wR + 0.7f * wL;

        float mid  = 0.5f * (L + R);
        float side = 0.5f * (L - R);

        float wet = mid + side;

        return mixOut(x, wet);
    }

private:
    Delay dL, dR;

    float p1 = 0.f;
    float p2 = 0.f;

    float rate1 = 0.17f;
    float rate2 = 0.23f;

    float depth1 = 0.0035f;
    float depth2 = 0.0045f;

    int mode = 1;
};
