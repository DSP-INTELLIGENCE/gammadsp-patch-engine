#pragma once
#include "AnalogPole.hpp"

class AnalogLadder: public Function
{    
public:

    AnalogLadder(float fc, float r) {
        reset();
        set(fc,r);
    }
    virtual ~AnalogLadder() = default;

    void reset()
    {
        p1.reset(); p2.reset(); p3.reset(); p4.reset();
    }
    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    inline float driveComp() const {
        return 1.f / (1.f + 0.35f * drive);
    }

    void set(float fc, float res)
    {
        float sr = gam::sampleRate();        

        cutoff    = std::clamp(fc, 0.0001f * sr, 0.499f * sr);
        resonance = std::clamp(res, 0.f, 1.2f);

        k = 4.f * resonance;
        k = k / (1.f + 0.6f * k);
        k *= (0.5f + 0.5f * (cutoff / (0.5f * sr)));
            
        p1.setCut(cutoff, sr);
        p2.setCut(cutoff, sr);
        p3.setCut(cutoff, sr);
        p4.setCut(cutoff, sr);
    }

    inline float process(float x)
    {
        x *= drive;

        // transistor asymmetry
        x += 0.05f * x * x;

        float u = x;

        for (int it = 0; it < 2; ++it)
        {
            float y4 = p4.s;
            u = x - k * p4.nl(y4);

            float y1 = p1.process(u);
            float y2 = p2.process(y1);
            float y3 = p3.process(y2);
            p4.process(y3);
        }

        p1.s = zap(p1.s);
        p2.s = zap(p2.s);
        p3.s = zap(p3.s);
        p4.s = zap(p4.s);

        return dcBlock(p4.s) * driveComp();
    }


    AnalogPole p1, p2, p3, p4;

    float cutoff = 1000.f;
    float resonance = 0.5f;
    float drive = 1.f;

    float k = 0.f;
    float dc_x1 = 0.f, dc_y1 = 0.f;

    inline float dcBlock(float x){
        float y = x - dc_x1 + 0.995f * dc_y1;
        dc_x1 = x;
        dc_y1 = y;
        return y;
    }

};
