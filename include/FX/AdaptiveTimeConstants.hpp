#pragma once
#include <algorithm>
#include <cmath>


class AdaptiveTimeConstants {
public:
    void setBaseAttack(float sec)  { baseAtk = std::max((float)1e-6, sec); }
    void setBaseRelease(float sec) { baseRel = std::max((float)1e-6, sec); }

    void setAdaptAmount(float a)   { adapt = std::clamp(a, (float)0, (float)1); }
    void setMemory(float m)        { memory = std::clamp(m, (float)0, (float)1); }

    void reset()
    {
        avgError = 0;
        compTime = 0;
    }

    void compute(float targetDb, float currentDb,
                 float& outAtk, float& outRel)
    {
        float error = std::abs(targetDb - currentDb);

        // Smoothed error (memory)
        avgError = (1 - memory) * error + memory * avgError;

        // Track how long compression is active
        if (targetDb < currentDb)
            compTime++;
        else
            compTime = 0;

        // Adaptation factor
        float eNorm = std::min((float)1, avgError / (float)12); // 12 dB ref
        float speedUp = (float)1 - adapt * eNorm;

        // Long compression → slower release
        float longComp = std::min((float)1, compTime / (float)(0.5 * gam::sampleRate()));

        outAtk = baseAtk * speedUp;
        outRel = baseRel * (1 + adapt * longComp);
    }

private:
    float baseAtk { (float)0.01 };
    float baseRel { (float)0.1 };

    float adapt { (float)0.5 };
    float memory { (float)0.9 };

    float avgError { 0 };
    int compTime { 0 };
};
