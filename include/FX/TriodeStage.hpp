// GDSP_TriodeStage.hpp
#pragma once
#include <cmath>
#include <algorithm>

struct OnePoleLP {
    float a = 0, z = 0;
    void set(float hz, float sr){
        a = std::exp(-2.f * 3.14159265359f * hz / sr);
    }
    float process(float x){
        z = a * z + (1.f - a) * x;
        return z;
    }
};

struct TriodeStage {
    float sr = 44100.f;

    // knobs 0..1
    float drive  = 0.5f;
    float tone   = 0.5f;
    float level  = 0.5f;
    float bias   = 0.15f;   // −0.15 .. +0.30
    float warmth = 0.6f;

    OnePoleLP toneLP;

    TriodeStage(float sampleRate){
        sr = sampleRate;
        toneLP.set(3000.f, sr);
    }

    float triodeShape(float x){
        float u = x + bias;

        float p = std::max(0.f, u);
        float n = std::max(0.f, -u);

        float yp = 1.0f - std::pow(1.f + p, -1.5f);
        float yn = -std::pow(n, 1.5f) * (0.6f + 0.4f * warmth);

        return yp + yn;
    }

    float process(float x){
        // Drive mapping: 0..+45 dB
        float driveDB = 45.f * drive * drive;
        float g = std::pow(10.f, driveDB / 20.f);

        // Level: −18..+12 dB
        float levelDB = -18.f + 30.f * level;
        float outGain = std::pow(10.f, levelDB / 20.f);

        // Tone: post low-pass
        float fc = 1000.f * std::pow(10.f, tone); // 1k → 10k
        toneLP.set(fc, sr);

        // signal path
        float y = x * g;
        y = triodeShape(y);
        y = toneLP.process(y);

        return y * outGain;
    }
};
