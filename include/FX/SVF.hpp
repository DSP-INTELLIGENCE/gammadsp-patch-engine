#pragma once
#include <cmath>
#include <algorithm>

#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

enum SVFType {
    SVFLowpass = 0,
    SVFBandpass,
    SVFHighpass,
    SVFNotch,
    SVFAllpass,
    SVFPeak
};

class SVF : public Function {
public:
    SVF(){
        updateCoeffs();
    }

    void setType(uint32_t newType){ type = (SVFType)newType; }

    void setCutoff(float newCutoff){
        cutoff = newCutoff;
        updateCoeffs();
    }

    void setQ(float newQ){
        Q = newQ;
        updateCoeffs();
    }

    void set(float newCutoff, float newQ){
        cutoff = newCutoff;
        Q = newQ;
        updateCoeffs();
    }

    void reset(){
        z1[0]=z1[1]=0.0f;
        z2[0]=z2[1]=0.0f;
    }

    void updateCoeffs(){
        const float fs = gam::sampleRate();

        cutoff = std::clamp(cutoff, 5.0f, 0.49f * fs);
        Q      = std::max(Q, 0.05f);

        g = std::tan(float(M_PI) * cutoff / fs);
        k = 1.0f / Q;

        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    float processChannel(float x, int ch){
        float ic1 = z1[ch];
        float ic2 = z2[ch];

        // ZDF core
        float v3 = x - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;

        z1[ch] = 2.0f * v1 - ic1;
        z2[ch] = 2.0f * v2 - ic2;

        float hp = x - k * v1 - v2;
        float bp = v1;
        float lp = v2;

        switch(type){
            case SVFLowpass:  return lp;
            case SVFBandpass: return bp;
            case SVFHighpass: return hp;
            case SVFNotch:    return hp + lp;
            case SVFAllpass:  return hp + lp - k * bp;
            case SVFPeak:     return lp - hp;
            default:          return lp;
        }
    }

    float process(float x) override { return processChannel(x,0); }
    float operator()(float x){ return process(x); }

private:
    SVFType type = SVFLowpass;

    float cutoff = 1000.0f;
    float Q = 0.707f;

    float g = 0.0f;
    float k = 0.0f;
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

    float z1[2] = {0,0};
    float z2[2] = {0,0};
};
