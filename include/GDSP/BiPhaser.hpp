#pragma once
#include "Engine.hpp"
#include "Parameters.hpp"
#include "Comb.hpp"

// Mutron Bi-Phase
class PhaserStage {
public:
    PhaserStage(int stages = 6)
    {
        filters.resize(stages);
        for (auto& f : filters) {
            f = std::make_unique<Comb>(0.01f);
            f->setAllPass(0.7f);
            f->setFeedback(0.0f);
            f->setIpolType(gam::ipl::Type::LINEAR);
        }
    }

    void setFeedback(float fb)
    {
        for (auto& f : filters)
            f->setFeedback(fb);
    }

    float process(float x, float delay)
    {
        float y = x;
        for (auto& f : filters) {
            f->setDelay(delay);
            y = f->process(y);
        }
        return y;
    }

private:
    std::vector<std::unique_ptr<Comb>> filters;
};

class MutronBiPhaser : public Function {
public:
    MutronBiPhaser()
    : phaserA(6), phaserB(6)
    {
        depth = 0.002f;
        feedback = 0.7f;
        mix = 0.6f;
    }

    float process(float x) override
    {
        float dA = baseDelay + depth * lfoA.process();
        float dB = baseDelay + depth * lfoB.process();

        phaserA.setFeedback(feedback);
        phaserB.setFeedback(feedback);

        float yA = phaserA.process(x, dA);
        float yB = phaserB.process(yA, dB);

        return x * (1.0f - mix) + yB * mix;
    }

    void setDepth(float d) { depth = d; }
    void setFeedback(float f) { feedback = f; }
    void setMix(float m) { mix = m; }
    void setLFOA(float v) { lfoA.set(v); }
    void setLFOB(float v) { lfoB.set(v); }

    Modulator lfoA, lfoB;

private:
    PhaserStage phaserA, phaserB;
    float depth = 0.002f;
    float baseDelay = 0.002f;
    float feedback = 0.7f;
    float mix = 0.6f;    
};