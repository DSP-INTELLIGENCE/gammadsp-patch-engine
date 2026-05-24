#pragma once
#include "TPT1Pole.hpp"
#include "Triode.hpp"

struct TriodePole
{
    float sr = 44100.f;

    Triode triode;

    TPT1Pole inputHP;     // coupling cap / DC block
    TPT1Pole poleLP;      // the ladder pole (TPT)

    // Tube memory / bias drift (local to the stage)
    float bias = 0.f;
    float biasAmount = 0.25f;   // how much envelope shifts bias
    float biasRate   = 0.01f;   // recovery speed (simple 1-pole)

    // Drive into the grid
    float drive = 1.0f;

    void init(float fs)
    {
        sr = fs;
        inputHP.setHPCut(15.f, sr);
        poleLP.setLPCut(1200.f, sr);
        reset();
    }

    void reset()
    {
        inputHP.reset();
        poleLP.reset();
        bias = 0.f;
    }

    void setPoleCut(float fc)
    {
        poleLP.setLPCut(fc, sr);
    }

    inline float process(float x, float fbToGrid = 0.f)
    {
        // 1) coupling cap
        x = inputHP.processHP(x);

        // 2) envelope -> bias drift (tube memory)
        float env = fabsf(x);
        float targetBias = -biasAmount * env;
        bias += biasRate * (targetBias - bias);

        // 3) grid voltage (feedback shifts operating point)
        float vgk = x * drive + bias - fbToGrid;

        // 4) plate voltage proxy (use pole state as “plate-ish”)
        float vpk = std::max(0.1f, poleLP.s);

        // 5) triode current
        float ip = triode.process(vgk, vpk);

        // 6) pole integrates “-ip” (current into RC)
        float y = poleLP.processLP(-ip);

        return y;
    }
};
