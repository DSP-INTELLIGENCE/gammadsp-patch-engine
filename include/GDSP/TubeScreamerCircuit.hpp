#pragma once
#include "DistortionCircuits.hpp"

class TubeScreamerCircuit : public DistortionCircuit {
public:
    void prepare(float sampleRate) override {
        sr = sampleRate;

        // Device model
        shaper.setMode(Waveshaper::WS_SOFTEXP);

        // Core analog stage
        stage.init(sr, &shaper);

        // TS-9 style defaults
        stage.couplingHP.setHPCut(100.f, sr);
        stage.envLP.setLPCut(18.f, sr);
        stage.storageLP.setLPCut(6.f, sr);

        stage.baseBias   = 0.f;
        stage.biasAmount = 0.15f;
        stage.sagAmount  = 0.08f;

        // Tone network (post stage)
        toneLP.setLPCut(1800.f, sr);
        toneHP.setHPCut(720.f, sr);

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
        float dB = 40.f * drive01 * drive01;
        stage.drive = std::pow(10.f, dB / 20.f);

        // --- Core nonlinear stage ---
        float y = stage.process(x);

        // --- Tone control ---
        float dark   = toneLP.processLP(y);
        float bright = toneHP.processHP(y);
        y = (1.f - tone01) * dark + tone01 * bright;

        // --- Level ---
        float levelDB = -18.f + 36.f * level01;
        float gain = std::pow(10.f, levelDB / 20.f);

        return y * gain;
    }

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "Tube Screamer TS-9"; }

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


