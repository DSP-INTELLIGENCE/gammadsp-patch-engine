#pragma once
#include "DistortionCircuits.hpp"

class OCDCircuit : public DistortionCircuit {
public:
    void prepare(float sampleRate) override {
        sr = sampleRate;

        // OCD uses hard-ish MOSFET clipping with amp-like response
        shaper.setMode(Waveshaper::WS_ARCCLIP);

        stage.init(sr, &shaper);

        // OCD voicing: open lows, strong punch, moderate compression
        stage.couplingHP.setHPCut(45.f, sr);
        stage.envLP.setLPCut(14.f, sr);
        stage.storageLP.setLPCut(5.f, sr);

        stage.baseBias   = 0.f;
        stage.biasAmount = 0.12f;
        stage.sagAmount  = 0.10f;

        // Tone network
        toneLP.setLPCut(9000.f, sr);
        toneHP.setHPCut(650.f, sr);

        // HP/LP toggle emulation: default HP mode
        hpMode = true;

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
        // Drive: 0 → +50 dB
        float dB = 50.f * drive01 * drive01;
        stage.drive = std::pow(10.f, dB / 20.f);

        float y = stage.process(x);

        // OCD tone shaping
        float dark   = toneLP.processLP(y);
        float bright = toneHP.processHP(y);

        // HP / LP voicing switch
        float t = tone01;
        if (hpMode)
            y = (1.f - t) * dark + t * bright;
        else
            y = (1.f - t) * bright + t * dark;

        // Level: -18 → +12 dB
        float levelDB = -18.f + 30.f * level01;
        float gain = std::pow(10.f, levelDB / 20.f);

        return y * gain;
    }

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    void setHPMode(bool on) { hpMode = on; }

    std::string name() const override { return "Fulltone OCD"; }

private:
    float sr = 44100.f;

    AnalogStage stage;
    Waveshaper shaper;

    TPT1Pole toneLP;
    TPT1Pole toneHP;

    bool hpMode = true;

    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;
};


