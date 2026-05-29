#pragma once
#include "DistortionCircuits.hpp"

class KlonCircuit : public DistortionCircuit {
public:
    void prepare(float sampleRate) override {
        sr = sampleRate;

        // Transparent soft clipping
        shaper.setMode(Waveshaper::WS_SOFTEXP);

        stage.init(sr, &shaper);

        // Klon voicing
        stage.couplingHP.setHPCut(90.f, sr);
        stage.envLP.setLPCut(16.f, sr);
        stage.storageLP.setLPCut(6.f, sr);

        stage.baseBias   = 0.f;
        stage.biasAmount = 0.10f;
        stage.sagAmount  = 0.05f;

        // Tone: gentle presence control
        toneLP.setLPCut(9000.f, sr);
        toneHP.setHPCut(900.f, sr);

        setDrive(0.5f);
        setTone(0.5f);
        setLevel(0.5f);
        cleanMix = 0.65f; // hallmark Klon blend
    }

    void reset() override {
        stage.reset();
        toneLP.reset();
        toneHP.reset();
    }

    float process(float x) override {
        // Drive: 0 → +35 dB
        float dB = 35.f * drive01 * drive01;
        stage.drive = std::pow(10.f, dB / 20.f);

        // Distorted path
        float dirty = stage.process(x);

        // Clean path (buffered, slightly bright)
        float clean = x * 1.05f;

        // Blend
        float y = cleanMix * clean + (1.f - cleanMix) * dirty;

        // Tone
        float dark   = toneLP.processLP(y);
        float bright = toneHP.processHP(y);
        y = (1.f - tone01) * dark + tone01 * bright;

        // Level: -18 → +12 dB
        float levelDB = -18.f + 30.f * level01;
        float gain = std::pow(10.f, levelDB / 20.f);

        return y * gain;
    }

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "Klon Centaur"; }

private:
    float sr = 44100.f;

    AnalogStage stage;
    Waveshaper shaper;

    TPT1Pole toneLP;
    TPT1Pole toneHP;

    float cleanMix = 0.65f;

    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;
};


