#pragma once
#include "GDSP_Waveshaper.hpp

class ZDF_SVF
{
public:
    float drive = 1.0f;

    ZDF_SVF(float fc, float Q = 0.707f) {
        reset();
        set(fc,Q);
    }
    void reset(){ ic1 = ic2 = 0.f; }

    void set(float fc, float Q)
    {
        float sr = gam::sampleRate();
        fc = std::clamp(fc, 0.0001f * sr, 0.499f * sr);
        Q  = std::max(0.05f, Q);

        R = 1.f / (2.f * Q);
        R = R / (1.f + 0.5f * R);

        const float T  = 1.f / sr;
        const float wd = 2.f * TPT1Pole::kPi * fc;
        const float wa = (2.f / T) * std::tan(wd * T * 0.5f);
        g = std::min(wa * T * 0.5f, 1000.f);
    }
    inline void setFast(float gNew, float RNew){
        g = gNew;
        R = RNew;
    }
    inline float driveComp() const {
        return 1.f / (1.f + 0.4f * drive);
    }

    inline float sat(float x) const { return std::tanh(x * drive); }

    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }
    float lp() const { return _lp; }
    float hp() const { return _hp; }
    float bp() const { return _bp; }
    float notch() const { return _notch; }

    inline float process(float x)
    {
        x = sat(x - 2.f * R * sat(_lp));
        x += 0.05f * x * x;   // subtle tube asymmetry

        const float h = 1.f / (1.f + 2.f * R * g + g * g);

        _hp = (x - ic2) * h;
        _bp = g * _hp + ic1;
        _lp = g * _bp + ic2;

        ic1 = zap(g * _hp + _bp);
        ic2 = zap(g * _bp + _lp);

        _notch = _hp + _lp;
        return _lp * driveComp();
    }

    float ic1=0, ic2=0;
    float g=0, R=0.5f;
    float _lp=0, _hp=0, _bp=0, _notch=0;
};


