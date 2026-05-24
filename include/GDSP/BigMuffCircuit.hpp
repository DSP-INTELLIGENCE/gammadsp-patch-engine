#pragma once
#include "DistortionCircuits.hpp"

class BigMuffCircuit : public DistortionCircuit {
public:
    void prepare(float sampleRate) override {
        sr = sampleRate;

        // ===== Stage 1: Input fuzz =====
        shaper1.setMode(Waveshaper::WS_CUBASIC);
        stage1.init(sr, &shaper1);

        stage1.couplingHP.setHPCut(10.f, sr);
        stage1.envLP.setLPCut(10.f, sr);
        stage1.storageLP.setLPCut(4.f, sr);

        stage1.baseBias   = 0.f;
        stage1.biasAmount = 0.30f;
        stage1.sagAmount  = 0.20f;

        // ===== Stage 2: Sustain fuzz =====
        shaper2.setMode(Waveshaper::WS_HARDCLIP);
        stage2.init(sr, &shaper2);

        stage2.couplingHP.setHPCut(10.f, sr);
        stage2.envLP.setLPCut(8.f, sr);
        stage2.storageLP.setLPCut(3.f, sr);

        stage2.baseBias   = 0.f;
        stage2.biasAmount = 0.25f;
        stage2.sagAmount  = 0.15f;

        // ===== Stage 3: Tone stack shaping =====
        toneLP.setLPCut(4000.f, sr);
        toneHP.setHPCut(800.f, sr);

        setDrive(0.5f);
        setTone(0.5f);
        setLevel(0.5f);
    }

    void reset() override {
        stage1.reset();
        stage2.reset();
        toneLP.reset();
        toneHP.reset();
    }

    float process(float x) override {
        // Drive: controls both fuzz stages
        float dB = 60.f * drive01 * drive01;
        float g = std::pow(10.f, dB / 20.f);

        stage1.drive = g;
        stage2.drive = g * 0.7f;

        float y = stage1.process(x);
        y = stage2.process(y);

        // Big Muff tone stack (mid scoop blend)
        float low  = toneLP.processLP(y);
        float high = toneHP.processHP(y);
        y = (1.f - tone01) * low + tone01 * high;

        // Level: -24 → +6 dB
        float levelDB = -24.f + 30.f * level01;
        float gain = std::pow(10.f, levelDB / 20.f);

        return y * gain;
    }

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "Big Muff Pi"; }

private:
    float sr = 44100.f;

    // Two fuzz stages
    AnalogStage stage1;
    AnalogStage stage2;

    Waveshaper shaper1;
    Waveshaper shaper2;

    // Tone stack
    TPT1Pole toneLP;
    TPT1Pole toneHP;

    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;
};


