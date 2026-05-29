#pragma once
#include "OTA1Pole.hpp"

class OTA_Ladder
{    
public:

    OTA_Ladder(float fc, float r) {
        reset();
        set(fc, r);
    }

    void reset()
    {
        p1.reset(); p2.reset(); p3.reset(); p4.reset();
    }

    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    void set(float fc, float res)
    {
        float sr = gam::sampleRate();        

        cutoff    = std::clamp(fc, 0.0001f * sr, 0.499f * sr);
        resonance = std::clamp(res, 0.f, 1.2f);

        k = 4.f * resonance;
        k = k / (1.f + 0.6f * k);
        k *= (0.5f + 0.5f * (cutoff / (0.5f * sr)));

        float gm = 1.f + 2.5f * drive;
        p1.gm = p2.gm = p3.gm = p4.gm = gm;

        p1.setCut(cutoff, sr);
        p2.setCut(cutoff, sr);
        p3.setCut(cutoff, sr);
        p4.setCut(cutoff, sr);
    }


    inline float process(float x)
    {
        x *= drive;

        float u = x - k * OTA_TPT1Pole::ota(p4.s, p4.gm);
        u = OTA_TPT1Pole::ota(u, p1.gm);

        float y1 = p1.process(u);
        float y2 = p2.process(y1);
        float y3 = p3.process(y2);
        float y4 = p4.process(y3);

        p1.s = zap(p1.s);
        p2.s = zap(p2.s);
        p3.s = zap(p3.s);
        p4.s = zap(p4.s);

        return dcBlock(y4);
    }

    OTA_TPT1Pole p1, p2, p3, p4;

    float cutoff = 1000.f;
    float resonance = 0.5f;
    float drive = 1.f;

    float k = 0.f;

    float dc_x1=0, dc_y1=0;
    inline float dcBlock(float x){
        float y = x - dc_x1 + 0.995f * dc_y1;
        dc_x1 = x; dc_y1 = y;
        return y;
    }

};


