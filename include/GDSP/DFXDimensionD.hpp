#pragma once
#include "DFXDigitalDelay.hpp"

class DFXDimensionD {
public:
    enum Mode { OFF, I, II, III, IV };

    DFXDimensionD()
    : d1(0.05f, 0.012f), d2(0.05f, 0.017f),
      d3(0.05f, 0.022f), d4(0.05f, 0.028f)
    {
        setMode(I);
    }

    void setMode(Mode m)
    {
        mode = m;

        switch (mode)
        {
            case I:   depth = 0.20f; rate = 0.10f; mix = 0.40f; break;
            case II:  depth = 0.30f; rate = 0.12f; mix = 0.45f; break;
            case III: depth = 0.45f; rate = 0.14f; mix = 0.50f; break;
            case IV:  depth = 0.60f; rate = 0.18f; mix = 0.55f; break;
            default: break;
        }
    }

    void process(float inL, float inR, float& outL, float& outR)
    {
        if (mode == OFF) { outL = inL; outR = inR; return; }

        // ultra-slow multi-phase modulation
        float m1 = std::sin(p1);
        float m2 = std::sin(p2 + 1.1f);
        float m3 = std::sin(p3 + 2.3f);
        float m4 = std::sin(p4 + 3.7f);

        p1 += 2.f * float(M_PI) * rate / gam::sampleRate();
        p2 += 2.f * float(M_PI) * rate * 0.97f / gam::sampleRate();
        p3 += 2.f * float(M_PI) * rate * 1.03f / gam::sampleRate();
        p4 += 2.f * float(M_PI) * rate * 1.01f / gam::sampleRate();

        // delay modulation
        d1.setDelay(0.012f + m1 * depth * 0.004f);
        d2.setDelay(0.017f + m2 * depth * 0.004f);
        d3.setDelay(0.022f + m3 * depth * 0.004f);
        d4.setDelay(0.028f + m4 * depth * 0.004f);

        // run delay lines
        float a = d1.process(inL);
        float b = d2.process(inR);
        float c = d3.process(inL);
        float d = d4.process(inR);

        // stereo dimension matrix
        float L = a + d * 0.6f;
        float R = b + c * 0.6f;

        // analog color
        L = std::tanh(L * 1.4f);
        R = std::tanh(R * 1.4f);

        // dry/wet blend
        outL = inL * (1.f - mix) + L * mix;
        outR = inR * (1.f - mix) + R * mix;
    }

private:
    DFXDigitalDelay d1, d2, d3, d4;

    Mode mode = OFF;
    float depth = 0.3f, rate = 0.12f, mix = 0.45f;

    float p1=0, p2=0, p3=0, p4=0;
};
