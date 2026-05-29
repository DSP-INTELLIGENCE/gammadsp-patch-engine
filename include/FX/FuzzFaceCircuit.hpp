#pragma once
#include "DistortionCircuits.hpp"

class FuzzFaceCircuit : public DistortionCircuit {
public:
    void prepare(float sampleRate) override {
        sr = sampleRate;

        // Fuzz Face: transistor-like nonlinear law
        shaper.setMode(Waveshaper::WS_TRIODE);  // asymmetric & wild

        stage.init(sr, &shaper);

        // Fuzz Face voicing
        stage.couplingHP.setHPCut(5.f, sr);
        stage.envLP.setLPCut(6.f, sr);
        stage.storageLP.setLPCut(2.f, sr);

        stage.baseBias   = -0.15f;   // unstable transistor bias
        stage.biasAmount = 0.50f;
        stage.sagAmount  = 0.30f;

        // Very simple tone (Fuzz Face is raw)
        toneLP.setLPCut(12000.f, sr);

        setDrive(0.5f);
        setTone(0.5f);
        setLevel(0.5f);
    }

    void reset() override {
        stage.reset();
        toneLP.reset();
    }

    float process(float x) override {
        // Drive: insane range (0 → +65 dB)
        float dB = 65.f * drive01 * drive01;
        stage.drive = std::pow(10.f, dB / 20.f);

        // Core fuzz
        float y = stage.process(x);

        // Simple brightness control
        float dark   = toneLP.processLP(y);
        y = (1.f - tone01) * dark + tone01 * y;

        // Level: -24 → +6 dB
        float levelDB = -24.f + 30.f * level01;
        float gain = std::pow(10.f, levelDB / 20.f);

        return y * gain;
    }

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "Fuzz Face"; }

private:
    float sr = 44100.f;

    AnalogStage stage;
    Waveshaper shaper;

    TPT1Pole toneLP;

    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;
};


