#pragma once
#include "MultiStagePhaser.hpp"
#include <array>

class TalkingPhaser : public Function
{
public:
    TalkingPhaser()
    {
        setVowel(0);          // "A"
        setRate(0.15f);
        setDepth(0.8f);
        setEnvAmount(0.6f);
        setFeedback(0.6f);
        setMix(0.7f);
    }

    void setRate(float r)      { phaser.setRate(r); }
    void setDepth(float d)     { phaser.setDepth(d); }
    void setEnvAmount(float e) { phaser.setEnvAmount(e); }
    void setFeedback(float f)  { phaser.setFeedback(f); }
    void setMix(float m)       { phaser.setMix(m); }

    void setVowel(int v)
    {
        current = v % vowels.size();
        targetFormant = vowels[current];
    }

    float process(float x) override
    {
        // Envelope-driven vowel motion
        motion += (envFollow(x) - motion) * 0.002f;

        // Morph toward target vowel
        for (int i = 0; i < 3; ++i)
            formant[i] += (targetFormant[i] - formant[i]) * 0.002f;

        // Use vowel center as phaser sweep driver
        float center = (formant[0] + formant[1] + formant[2]) * 0.33f;
        phaser.overrideBaseFreq(center);

        return phaser.process(x);
    }

private:
    float envFollow(float x)
    {
        float level = std::fabs(x);
        env += (level - env) * 0.01f;
        return env;
    }

private:
    MultiStagePhaser phaser;

    float env = 0.f;
    float motion = 0.f;

    std::array<std::array<float,3>, 5> vowels {{
        { 700, 1100, 2500 },   // A
        { 400, 1700, 2600 },   // E
        { 300, 2300, 3000 },   // I
        { 500,  900, 2400 },   // O
        { 350,  600, 2300 }    // U
    }};

    std::array<float,3> formant {500, 1200, 2500};
    std::array<float,3> targetFormant {500, 1200, 2500};

    int current = 0;
};
