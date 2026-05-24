#pragma once
#include "DistortionCircuits.hpp"

// assumes TPT1Pole + AnalogStage + Waveshaper exist
struct DiodeClipperCircuit : public DistortionCircuit
{
    float sr = 44100.f;

    Waveshaper  shaper;
    AnalogStage stage;

    TPT1Pole preLP;
    TPT1Pole postLP;

    // user parameters
    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;

    // smoothed controls
    float driveLP = 0.5f;
    float toneLP  = 0.5f;
    float levelLP = 0.5f;

    // DC blocker
    float dc_x1 = 0.f, dc_y1 = 0.f;

    // -------------------------------------------------

    void prepare(float sampleRate) override
    {
        sr = sampleRate;
        reset();

        stage.init(sr, shaper);

        stage.couplingHP.setHPCut(40.f, sr);
        stage.envLP.setLPCut(20.f, sr);
        stage.storageLP.setLPCut(3.f, sr);

        stage.baseBias   = 0.f;
        stage.biasAmount = 0.03f;
        stage.sagAmount  = 0.02f;

        preLP.setLPCut(7000.f, sr);
        postLP.setLPCut(6000.f, sr);

        shaper.setMode(Waveshaper::WS_ARCCLIP);
        shaper.setDrive(1.f);
        shaper.setCurve(1.f);
        shaper.setOutGain(1.f);
        shaper.setAM(1.f);
    }

    void reset() override
    {
        stage.reset();
        preLP.reset();
        postLP.reset();

        driveLP = drive01;
        toneLP  = tone01;
        levelLP = level01;

        dc_x1 = dc_y1 = 0.f;
    }

    // -------------------------------------------------

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "Diode Clipper"; }

    // -------------------------------------------------

    inline float zap(float x) const
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    inline float dcBlock(float x)
    {
        float y = x - dc_x1 + 0.995f * dc_y1;
        dc_x1 = x; dc_y1 = y;
        return y;
    }

    // -------------------------------------------------

    float process(float x) override
    {
        // smooth controls
        driveLP += 0.002f * (drive01 - driveLP);
        toneLP  += 0.002f * (tone01  - toneLP);
        levelLP += 0.002f * (level01 - levelLP);

        // --- control mapping ---

        // Drive: 0..+50 dB
        float dB = 50.f * (driveLP * driveLP);
        stage.drive = std::pow(10.f, dB / 20.f);

        // Tone control
        float postHz = 1800.f * std::pow(10.f, std::log10(9000.f / 1800.f) * toneLP);
        postLP.setLPCut(postHz, sr);

        // Output level: -24..+6 dB
        float leveldB = -24.f + 30.f * levelLP;
        float outGain = std::pow(10.f, leveldB / 20.f);

        // -------------------------------------------------
        // Circuit flow
        // -------------------------------------------------

        x = stage.couplingHP.processHP(x);
        x = preLP.processLP(x);

        float env = stage.envLP.processLP(std::fabs(x));

        float targetBias = stage.baseBias - stage.biasAmount * env;
        stage.bias = stage.storageLP.processLP(targetBias);
        stage.bias = zap(stage.bias);

        float y = stage.shaper.process(x * stage.drive + stage.bias);

        float sag = 1.f / (1.f + stage.sagAmount * env);
        y *= sag;

        y = postLP.processLP(y);

        return dcBlock(zap(y * outGain));
    }
};
