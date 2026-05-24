#pragma once
#include "TPT1Pole.hpp"

class ZDF_Ladder
{    
public:

    ZDF_Ladder(float cutoff, float res) {
        reset();
        set(cutoff,res);
    }
    // cheap, good-sounding saturation (replace with your Waveshaper if desired)
    static inline float sat(float x)
    {
        return std::tanh(x);
    }
    
    void reset()
    {
        p1.reset(); p2.reset(); p3.reset(); p4.reset();
    }
    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    void set(float fc, float res01)
    {
        float sr = gam::sampleRate();

        cutoffHz = fc;
        resonance = std::clamp(res01, 0.f, 1.2f); // allow a bit >1 if you like
        k = 4.f * resonance;
        k *= (0.5f + 0.5f * (cutoffHz / (0.5f * sr)));

        // set the same cutoff on all 4 poles
        p1.setCut(fc, sr);
        p2.setCut(fc, sr);
        p3.setCut(fc, sr);
        p4.setCut(fc, sr);

        // cache g from one pole
        g = p1.g;
    }

    inline float dcBlock(float x){
        float y = x - dc_x1 + 0.995f * dc_y1;
        dc_x1 = x;
        dc_y1 = y;
        return y;
    }
    inline float driveComp() const {
        return 1.f / (1.f + 0.4f * drive);
    }

    inline float process(float x)
    {
        x *= drive;

        const float yfb = p4.s;
        const float u0  = x - k * sat(yfb);
        const float u   = sat(u0 / (1.f + k * g * g * g * g));

        float v = u + 0.05f * u * u;
        
        float y1 = sat(p1.processLP(v));
        float y2 = sat(p2.processLP(y1));
        float y3 = sat(p3.processLP(y2));
        float y4o = p4.processLP(y3);

        p1.s = zap(p1.s);
        p2.s = zap(p2.s);
        p3.s = zap(p3.s);
        p4.s = zap(p4.s);

        return dcBlock(y4o) * driveComp();
    }

    
    TPT1Pole p1, p2, p3, p4;

    float cutoffHz = 1000.f;
    float resonance = 0.0f;   // 0..1-ish
    float drive = 1.0f;       // input drive
    float g = 0.f;
    float k = 0.f;            // feedback amount (≈ 4*res)
    float dc_x1 = 0.f, dc_y1 = 0.f;

       
};



