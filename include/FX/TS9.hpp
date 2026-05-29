// GDSP_TS9.hpp
#pragma once
#include <cmath>
#include <algorithm>

struct OnePoleLP {
    float a = 0, z = 0;
    void set(float hz, float sr){
        float x = std::exp(-2.f * 3.14159265359f * hz / sr);
        a = x;
    }
    float process(float x){
        z = a * z + (1.f - a) * x;
        return z;
    }
};

struct OnePoleHP {
    float a = 0, x1 = 0, y1 = 0;
    void set(float hz, float sr){
        a = std::exp(-2.f * 3.14159265359f * hz / sr);
    }
    float process(float x){
        float y = a * (y1 + x - x1);
        x1 = x; y1 = y;
        return y;
    }
};

class TS9 {
public:
    TS9(float sr){
        sampleRate = sr;
        inputHP.set(90.f, sr);
        toneLP.set(1800.f, sr);
        toneHP.set(720.f, sr);
    }

    void setDrive(float d){ drive = std::clamp(d,0.f,1.f); }
    void setTone (float t){ tone  = std::clamp(t,0.f,1.f); }
    void setLevel(float l){ level = std::clamp(l,0.f,1.f); }

    float process(float x){
        // Knob mappings
        float driveDB = 40.f * drive * drive;
        float gain    = std::pow(10.f, driveDB / 20.f);

        float levelDB = -18.f + 36.f * level;
        float outGain = std::pow(10.f, levelDB / 20.f);

        // Pre-conditioning
        x = inputHP.process(x);
        x *= gain;

        // Feedback-style saturation (soft, smooth, TS signature)
        float y = std::tanh(x * 1.5f);

        // Tone network (TS style)
        updateTone();
        float dark   = toneLP.process(y);
        float bright = toneHP.process(y);
        y = (1.f - tone) * dark + tone * bright;

        return y * outGain;
    }

private:
    void updateTone(){
        float t = tone;
        float lpHz = 1000.f * std::pow(10.f, 1.0f * t);   // 1k → 10k
        float hpHz = 400.f  * std::pow(10.f, 0.8f * t);  // 400 → 2.5k
        toneLP.set(lpHz, sampleRate);
        toneHP.set(hpHz, sampleRate);
    }

    float sampleRate = 44100.f;
    float drive = 0.5f, tone = 0.5f, level = 0.5f;

    OnePoleHP inputHP;
    OnePoleLP toneLP;
    OnePoleHP toneHP;
};
