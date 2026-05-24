#pragma once

struct AnalogVariation
{
    float temp = 0.f;
    float driftPhase = 0.f;

    // component tolerances
    float cutoffTol = 1.f;
    float resTol    = 1.f;
    float driveTol  = 1.f;

    void reset()
    {
        temp = 0.f;
        driftPhase = 0.f;

        // 1–2% random tolerance
        cutoffTol = 1.f + randomUniform(-0.015f, 0.015f);
        resTol    = 1.f + randomUniform(-0.02f,  0.02f);
        driveTol  = 1.f + randomUniform(-0.02f,  0.02f);
    }

    float process()
    {
        // very slow thermal wander
        driftPhase += 0.00002f;
        temp += 0.0001f * std::sin(driftPhase);

        return temp;
    }

    static float randomUniform(float a, float b)
    {
        return a + (b - a) * (float(rand()) / float(RAND_MAX));
    }
};
