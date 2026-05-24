// GDSP_TubeShaper.hpp
#pragma once
#include <cmath>
#include <algorithm>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

class TubeShaper : public Function {
public:
    TubeShaper(float drive=1.0f, float bias=0.0f, float curve=2.0f, float outGain=1.0f)
    {
        setDrive(drive);
        setBias(bias);
        setCurve(curve);
        setOutGain(outGain);
        setAM(1.0f);
    }

    // ---------- Controls ----------
    void setDrive(float v)   { mDrive = std::max(0.f, v); m_drive.set(mDrive); }
    void setBias(float v)    { mBias  = std::clamp(v, -0.5f, 0.5f); m_bias.set(mBias); }
    void setCurve(float v)   { mCurve = std::max(0.01f, v); m_curve.set(mCurve); }
    void setOutGain(float v) { mOutGain = std::max(0.f, v); m_gain.set(mOutGain); }
    void setAM(float v)      { m_am.set(v); }

    // ---------- Processing ----------
    float process(float input) override {
        float d = m_drive.process();
        float b = m_bias.process();
        float c = m_curve.process();
        float g = m_gain.process();
        float a = m_am.process();

        d = std::max(0.f, d);
        b = std::clamp(b, -0.5f, 0.5f);
        c = std::max(0.01f, c);
        g = std::max(0.f, g);

        // Preamp stage with DC bias
        float x = input * d + b;

        // Asymmetrical nonlinear tube curve
        float y;
        if(x >= 0.f){
            // Softer on positive swing
            y = 1.0f - std::exp(-x * c);
        } else {
            // Harder on negative swing
            y = - (1.0f - std::exp(x * c * 1.3f));
        }

        // Remove bias-induced DC offset
        y -= b * 0.6f;

        return y * g * a;
    }

    void run(const float* in, float* out, size_t n){
        for(size_t i=0;i<n;i++)
            out[i] = process(in[i]);
    }

    float mDrive   = 1.0f;
    float mBias    = 0.0f;
    float mCurve   = 2.0f;
    float mOutGain = 1.0f;

    Modulator m_drive;
    Modulator m_bias;
    Modulator m_curve;
    Modulator m_gain;
    Modulator m_am;
};
