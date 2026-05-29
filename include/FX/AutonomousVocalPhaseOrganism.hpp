#pragma once
#include <random>
#include "FFXFormantPhaseHybrid.hpp"
#include "GDSP_ModLFO.hpp"
#include "GDSP_EnvFollow.hpp"

class AutonomousVocalPhaseOrganism : public Function
{
public:
    AutonomousVocalPhaseOrganism()
    : engine(6),
      rng(std::random_device{}())
    {
        setEnergy(0.6f);
        setMood(0.5f);
        setComplexity(0.5f);
        setStability(0.5f);
        setPresence(0.8f);
    }

    // ---------- Macro Controls ----------
    void setEnergy(float v)     { energy = std::clamp(v, 0.f, 1.f); }
    void setMood(float v)       { mood = std::clamp(v, 0.f, 1.f); }
    void setComplexity(float v) { complexity = std::clamp(v, 0.f, 1.f); }
    void setStability(float v)  { stability = std::clamp(v, 0.f, 1.f); }
    void setPresence(float v)   { presence = std::clamp(v, 0.f, 1.f); }

    float process(float x) override
    {
        evolve();
        return engine.process(x) * presence;
    }

private:
    FormantPhaseHybrid engine;
    ModLFO lfoA, lfoB, lfoC;
    EnvFollow env;

    float energy     = 0.5f;
    float mood       = 0.5f;
    float complexity = 0.5f;
    float stability  = 0.5f;
    float presence   = 0.8f;

    std::mt19937 rng;

    void evolve()
    {
        float chaos = complexity * (1.f - stability);

        lfoA.setFreq(0.03f + chaos * 0.2f);
        lfoB.setFreq(0.05f + chaos * 0.3f);
        lfoC.setFreq(0.08f + chaos * 0.4f);

        float a = lfoA.process();
        float b = lfoB.process();
        float c = lfoC.process();

        engine.setRate(0.1f + a * energy);
        engine.setDepth(0.4f + b * energy);
        engine.setFeedback(0.2f + c * energy * 0.6f);

        // Vowel evolution
        if ((rng() & 1023) == 0)
        {
            auto v = static_cast<FormantMorphFilter::Voice>((rng() % 3));
            auto p = static_cast<FormantMorphFilter::Phoneme>((rng() % 10));
            engine.setVowel(v, p);
        }

        engine.setMorphSpeed(0.0005f + chaos * 0.01f);
    }
};
