#pragma once
#include "Delay.hpp"
#include "DFXCore.hpp"

class DFXDimensionDChorus : public DFXCore
{
public:
    DFXDimensionDChorus()
    : d1(0.05f, 0.020f),
      d2(0.05f, 0.021f),
      d3(0.05f, 0.022f),
      d4(0.05f, 0.023f)
    {
        d1.setIpolType(gam::ipl::Type::CUBIC);
        d2.setIpolType(gam::ipl::Type::CUBIC);
        d3.setIpolType(gam::ipl::Type::CUBIC);
        d4.setIpolType(gam::ipl::Type::CUBIC);

        setWet(1.0f);
        setDry(1.0f);
        setMix(0.55f);
    }

    // The real unit exposes only intensity and mix
    void setIntensity(float v)
    {
        intensity = std::clamp(v, 0.f, 1.f);

        // extremely shallow modulation
        depth = 0.00035f + 0.0008f * intensity;
    }

    float process(float x) override
    {
        float sr = gam::sampleRate();

        // Quad-phase LFO network
        float p0 = sinf(phase);
        float p1 = sinf(phase + 0.5f * float(M_PI));
        float p2 = sinf(phase + float(M_PI));
        float p3 = sinf(phase + 1.5f * float(M_PI));

        phase += 2.f * float(M_PI) * 0.18f / sr; // fixed slow movement
        if (phase > 2.f * float(M_PI)) phase -= 2.f * float(M_PI);

        // base delays
        float t1 = 0.020f + depth * p0;
        float t2 = 0.021f + depth * p1;
        float t3 = 0.022f + depth * p2;
        float t4 = 0.023f + depth * p3;

        d1.setDelay(t1);
        d2.setDelay(t2);
        d3.setDelay(t3);
        d4.setDelay(t4);

        float a = d1.process(x);
        float b = d2.process(x);
        float c = d3.process(x);
        float d = d4.process(x);

        // Dimension spatial projection
        float L =  0.50f*a + 0.40f*b - 0.30f*c + 0.20f*d;
        float R =  0.20f*a - 0.30f*b + 0.40f*c + 0.50f*d;

        float mid  = 0.5f * (L + R);
        float side = 0.5f * (L - R);

        float wet = mid + side;

        return mixOut(x, wet);
    }

private:
    Delay d1, d2, d3, d4;

    float phase = 0.f;
    float intensity = 0.5f;
    float depth = 0.0006f;
};
