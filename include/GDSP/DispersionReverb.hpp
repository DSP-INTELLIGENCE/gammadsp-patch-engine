#pragma once
#include <algorithm>
#include <cmath>
#include "Engine.hpp"
#include "Parameters.hpp"
#include "DispersionNetwork.hpp"
#include "AllPass1.hpp"
#include "OnePole.hpp"

class DispersionReverb : public Function
{
public:
    DispersionReverb()
    : air(9000.f, gam::LOW_PASS),
      body(90.f, gam::HIGH_PASS)
    {
        // Parallel material bodies
        for (int i = 0; i < 4; ++i)
            nets[i] = std::make_unique<DispersionNetwork>(3, 8);

        // Pre diffusion
        for (auto& ap : preDiff)
            ap.setFreq(1200.f);

        // Late diffusion
        for (auto& ap : lateDiff)
            ap.setFreq(900.f);

        setSize(0.7f);
        setDecay(0.85f);
        setMaterial(0.75f);
        setMotion(0.5f);
        setAir(0.6f);
        setMix(0.4f);
    }

    // -------- Controls --------

    void setMix(float v) { mix = std::clamp(v, 0.f, 1.f); }

    void setSize(float v)
    {
        size = std::clamp(v, 0.f, 1.f);
        for (int i = 0; i < 4; ++i)
            nets[i]->setFeedback(0.5f + size * 0.45f);
    }

    void setDecay(float v)
    {
        decay = std::clamp(v, 0.f, 1.f);
        for (int i = 0; i < 4; ++i)
            nets[i]->setFeedback(0.4f + decay * 0.55f);
    }

    void setMaterial(float v)
    {
        material = std::clamp(v, 0.f, 1.f);
        for (int i = 0; i < 4; ++i)
            nets[i]->setDispersion(0.3f + material * 0.7f);
    }

    void setMotion(float v)
    {
        motion = std::clamp(v, 0.f, 1.f);
        for (int i = 0; i < 4; ++i)
        {
            nets[i]->setDepth(motion);
            nets[i]->setRate(0.02f + motion * 0.12f);
        }
    }

    void setAir(float v)
    {
        airAmt = std::clamp(v, 0.f, 1.f);
        air.setFreq(3000.f + airAmt * 10000.f);
        body.setFreq(40.f + (1.f - airAmt) * 120.f);
    }

    // -------- Audio --------

    float process(float x) override
    {
        float in = x;

        // Pre diffusion
        for (auto& ap : preDiff)
            in = ap.process(in);

        // Parallel dispersive material
        float sum = 0.f;
        for (int i = 0; i < 4; ++i)
            sum += nets[i]->process(in);

        sum *= 0.25f;

        // Late diffusion
        for (auto& ap : lateDiff)
            sum = ap.process(sum);

        // Physical loss
        sum = body.process(sum);
        sum = air.process(sum);

        return x * (1.f - mix) + sum * mix;
    }

private:
    std::unique_ptr<DispersionNetwork> nets[4];

    AllPass1 preDiff[3];
    AllPass1 lateDiff[4];

    OnePole air;
    OnePole body;

    float mix = 0.4f;
    float size = 0.7f;
    float decay = 0.85f;
    float material = 0.75f;
    float motion = 0.5f;
    float airAmt = 0.6f;
};
