#pragma once
#include <cmath>
#include <algorithm>
#include "EchoMachine.hpp"
#include "PeakDetector.hpp"
#include "GainController.hpp"
#include "OnePole.hpp"

// ------------------------------------------------------
// Echo Compressor Machine
// ------------------------------------------------------
class EchoCompressorMachine : public StereoFunction {
public:
    EchoCompressorMachine(float maxDelaySeconds = 2.0f)
    : echo(maxDelaySeconds),
      scHPF(120.0f)
    {
        setCompThreshold(-18.0f);
        setCompRatio(4.0f);
        setCompAttack(0.005f);
        setCompRelease(0.120f);
        setDuckAmount(0.6f);
        setMakeup(0.0f);
    }

    // ---------------- Compression Controls ----------------

    void setCompThreshold(float db) { thresholdDb = db; }
    void setCompRatio(float r)      { ratio = std::max(1.0f, r); }

    void setCompAttack(float sec)   { gainCtrl.setAttack(sec); }
    void setCompRelease(float sec)  { gainCtrl.setRelease(sec); }

    void setDuckAmount(float a)     { duck = std::clamp(a, 0.0f, 1.0f); }
    void setMakeup(float db)        { makeupDb = db; }

    void setSidechainHPF(float hz)  { scHPF.setFreq(std::max(20.0f, hz)); }

    // ---------------- Delay Controls ----------------

    EchoMachine& delay() { return echo; }

    // ---------------- DSP ----------------

    inline StereoSample process(float in)
    {
        return processStereo(in, in);
    }

    inline StereoSample processStereo(float inL, float inR)
    {
        // --- Sidechain signal ---
        float sc =
            0.5f * (std::fabs(inL) + std::fabs(inR)) +
            0.5f * (std::fabs(lastFbL) + std::fabs(lastFbR));

        sc = scHPF.process(sc);

        // --- Level detection ---
        float env = detector.process(sc);
        float envDb = 20.0f * std::log10(std::max(env, 1e-12f));

        // --- Gain computer (downward compression) ---
        float over = envDb - thresholdDb;
        float targetGR = (over > 0.0f)
            ? -over * (1.0f - 1.0f / ratio)
            : 0.0f;

        // Ducking: input reduces feedback more aggressively
        targetGR *= (1.0f + duck);

        // --- Smooth gain reduction ---
        float grDb = gainCtrl.process(targetGR);
        float compGain = std::pow(10.0f, (grDb + makeupDb) / 20.0f);

        // --- Apply compression to feedback path ---
        echoFeedbackGain = compGain;

        // --- Process delay ---
        StereoSample out = echo.processStereo(inL, inR);

        // capture last feedback taps for next sidechain
        lastFbL = echo.getLastWetL() * echoFeedbackGain;
        lastFbR = echo.getLastWetR() * echoFeedbackGain;

        return out;
    }

private:
    EchoMachine echo;

    // Compressor internals
    PeakDetector<float> detector;
    GainController<float> gainCtrl;
    OnePole scHPF;

    // Compression params
    float thresholdDb = -18.0f;
    float ratio = 4.0f;
    float duck = 0.6f;
    float makeupDb = 0.0f;

    // Feedback modulation
    float echoFeedbackGain = 1.0f;

    // State
    float lastFbL = 0.0f;
    float lastFbR = 0.0f;
};
