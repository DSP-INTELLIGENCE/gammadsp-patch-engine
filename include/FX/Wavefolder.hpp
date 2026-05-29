// GDSP_Wavefolder.hpp
#pragma once
#include <cmath>
#include <algorithm>


class Wavefolder : public Function {
public:

    Wavefolder(float drive=1.0f, float outGain=1.0f, float softness=0.0f, int stages=1)
     {
        setDrive(drive);
        setOutGain(outGain);
        setSoftness(softness);
        setStages(stages);
        setAM(1.0f);
    }

    void setDrive(float v)   { mDrive = std::max(0.f, v); m_drive.set(mDrive); }
    void setOutGain(float v) { mOutGain = std::max(0.f, v); m_gain.set(mOutGain); }
    void setSoftness(float v){ mSoftness = std::clamp(v, 0.f, 1.f); m_soft.set(mSoftness); }
    void setStages(int s)    { mStages = std::max(1, s); }
    void setAM(float v)      { m_am.set(v); }
    
    float process(float input) override {
        float d  = m_drive.process();
        float g  = m_gain.process();
        float sf = m_soft.process();
        float _am = m_am.process();

        d  = std::max(0.f, d);
        g  = std::max(0.f, g);
        sf = std::clamp(sf, 0.f, 1.f);

        float y = input;

        // multi-stage fold
        float dd = d;
        for(int i=0;i<mStages;i++){
            y = fold_tri_(y * dd);
            dd *= 1.2f;
        }

        // optional softening
        if(sf > 0.f){
            float s = softclip_(y * 2.f);
            y = (1.f - sf) * y + sf * s;
        }

        return y * g * _am;
    }

    void run(const float* in, float* out, size_t n){
        for(size_t i=0;i<n;i++) out[i] = process(in[i]);
    }


    Modulator m_drive;
    Modulator m_gain;
    Modulator m_soft;
    Modulator m_am;

private:
    static inline float fold_tri_(float x){
        float y = std::fmod(x + 1.0f, 4.0f);
        if (y < 0.0f) y += 4.0f;
        y -= 2.0f;
        return (y < 0.f ? 1.f + y : 1.f - y);
    }

    static inline float softclip_(float x){
        return x / (1.0f + std::fabs(x));
    }

    float mDrive   = 1.0f;
    float mOutGain = 1.0f;
    float mSoftness= 0.0f;
    int   mStages  = 1;

};
