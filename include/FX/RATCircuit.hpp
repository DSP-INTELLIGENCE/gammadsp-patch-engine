#pragma once
#include "DistortionCircuits.hpp"

class RATCircuit : public DistortionCircuit {
public:
    void prepare(float sampleRate) override {
        sr = sampleRate;

        // RAT core: op-amp + hard-ish diodes
        shaper.setMode(Waveshaper::WS_ARCCLIP);

        stage.init(sr, &shaper);

        // RAT feel defaults
        stage.couplingHP.setHPCut(30.f, sr);
        stage.envLP.setLPCut(14.f, sr);
        stage.storageLP.setLPCut(5.f, sr);

        stage.baseBias   = 0.f;
        stage.biasAmount = 0.20f;
        stage.sagAmount  = 0.15f;

        // RAT "Filter" control (reverse tone)
        toneLP.setLPCut(6000.f, sr);
        toneHP.setHPCut(600.f, sr);

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
        // Drive: 0 → +55 dB
        float dB = 55.f * drive01 * drive01;
        stage.drive = std::pow(10.f, dB / 20.f);

        // Core distortion
        float y = stage.process(x);

        // RAT tone: inverted brightness control
        float dark   = toneLP.processLP(y);
        float bright = toneHP.processHP(y);
        float t = 1.f - tone01;  // RAT filter works backwards
        y = (1.f - t) * dark + t * bright;

        // Level: -24 → +6 dB
        float levelDB = -24.f + 30.f * level01;
        float gain = std::pow(10.f, levelDB / 20.f);

        return y * gain;
    }

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "ProCo RAT"; }

private:
    float sr = 44100.f;

    AnalogStage stage;
    Waveshaper shaper;

    TPT1Pole toneLP;
    TPT1Pole toneHP;

    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;
};



