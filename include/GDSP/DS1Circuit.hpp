#pragma once
#include "DistortionCircuits.hpp"

class DS1Circuit : public DistortionCircuit {
public:
    void prepare(float sampleRate) override {
        sr = sampleRate;

        // Device model: harder, more aggressive clipping
        shaper.setMode(Waveshaper::WS_ARCCLIP);

        // Core analog stage
        stage.init(sr, &shaper);

        // DS-1 style defaults
        stage.couplingHP.setHPCut(70.f, sr);
        stage.envLP.setLPCut(12.f, sr);
        stage.storageLP.setLPCut(3.f, sr);

        stage.baseBias   = 0.f;
        stage.biasAmount = 0.05f;
        stage.sagAmount  = 0.02f;

        // Tone network (DS-1 is bright & cutting)
        toneLP.setLPCut(5500.f, sr);
        toneHP.setHPCut(900.f, sr);

        setDrive(0.5f);
        setTone(0.5f);
        setLevel(0.5f);
    }

    void reset() override {
        stage.reset();
        toneLP.reset();
        toneHP.reset();
    }

    float process(float x) override {
        // --- Drive mapping ---
        float dB = 60.f * drive01 * drive01;
        stage.drive = std::pow(10.f, dB / 20.f);

        // --- Core nonlinear stage ---
        float y = stage.process(x);

        // --- Tone control ---
        float dark   = toneLP.processLP(y);
        float bright = toneHP.processHP(y);
        y = (1.f - tone01) * dark + tone01 * bright;

        // --- Level ---
        float levelDB = -24.f + 30.f * level01;
        float gain = std::pow(10.f, levelDB / 20.f);

        return y * gain;
    }

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "Boss DS-1"; }

private:
    float sr = 44100.f;

    AnalogStage stage;
    Waveshaper shaper;

    // Tone section
    TPT1Pole toneLP;
    TPT1Pole toneHP;

    // UI params
    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;
};


