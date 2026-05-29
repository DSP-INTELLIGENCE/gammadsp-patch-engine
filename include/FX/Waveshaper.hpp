// GDSP_Waveshaper.hpp
#pragma once
#include <cmath>
#include <algorithm>
#include <string>
#include <cmath>
#include <algorithm>

class Waveshaper : public Function {
public:
    Waveshaper(float drive=1.0f, float curve=1.0f, float outGain=1.0f)
    {
        setDrive(drive);
        setCurve(curve);
        setOutGain(outGain);
        setAM(1.0f);
    } 


    // -------- Controls --------
    void setDrive(float v)   { mDrive = std::max(0.f, v);   m_drive.set(mDrive); }
    void setCurve(float v)   { mCurve = std::max(0.01f, v); m_curve.set(mCurve); }  // >0
    void setOutGain(float v) { mOutGain = std::max(float(0), v); m_gain.set(mOutGain); }
    void setAM(float v)      { m_am.set(v); }


    // -------- Processing --------
    float process(float input) override {
        float d = m_drive.process();
        float c = m_curve.process();
        float g = m_gain.process();
        float _am = m_am.process();

        d = std::max(0.f, d);
        c = std::max(0.01f, c);
        g = std::max(0.f, g);

        // Pre-gain
        float x = input * d;

        // Nonlinear shaping
        float k = d * c;
        float y = shape(x, c) * shapeGainComp() * gainCompFromK(k) * _am;

        y = dcBlock(y);

        // Output gain
        return y * g;
    }

    void run(const float* in, float* out, size_t n){
        for(size_t i=0;i<n;i++)
            out[i] = process(in[i]);
    }

    float mDrive   = 1.0f;
    float mCurve   = 1.0f;
    float mOutGain = 1.0f;

    Modulator m_drive;
    Modulator m_curve;
    Modulator m_gain;
    Modulator m_am;

    enum WShape
    {        
        WS_LOGICCLIP,
        WS_HARDCLIP,
        WS_TANCLIP,
        WS_QUINTIC,
        WS_CUBASIC,
        WS_ALGCLIP,
        WS_ARCCLIP,
        WS_SINCLIP,
        WS_SOFTEXP,

        // --- Boutique additions ---
        WS_TRIODE,
        WS_DIODE,
        WS_TAPE,
        WS_FOLDBACK,
        WS_RECTIFIER,
        WS_PARABOLIC,
        WS_CHEBY3,
        WS_ASYMSCURVE,
        WS_POWER,
        WS_SOFTCRUSH
    };

    WShape waveshape = WS_TRIODE;
    void setMode(WShape m)   { waveshape = m; }     
    int sgn(float x) { return (float(0.0f) < x) - (x < float(0.0f));  }
    float logiclip(float x) noexcept { return 2.0f / (1.0f + std::exp(-2.0f * x)) - 1.0f; }
    float hardclip(float x) noexcept { return sgn(x) * fminf(fabsf(x), 1.0f); }
    float hardclip2(float x) noexcept { return std::tanh(float(4.0) * x); }

    float tanclip(float x) noexcept {
        float soft = 0.0f;
        return std::tanh((1.0f - 0.5f * soft) * x);
    }
    float quintic(float x) noexcept {
        if (fabsf(x) < 1.25f) {
            return x - (256.0f / 3125.0f) * powf(x, 5.0f);
        } else {
            return sgn(x) * 1.0f;
        }
    }
    float cubicBasic(float x) noexcept {
        if (fabsf(x) < 1.5f) {
            return x - (4.0f / 27.0f) * powf(x, 3.0f);
        } else {
            return sgn(x) * 1.0f;
        }
    }
    float algClip(float x) noexcept {
        float soft = 0.0f;
        return x / sqrtf((1.0f + 2.0f * soft + powf(x, 2.0f)));
    }
    float arcClip(float x) noexcept {
        float soft = 0.0f;
        return (2.0f / M_PI) * atanf((1.6f - soft * 0.6f) * x);
    }
    float sinclip(float x) noexcept {
        if (fabsf(x) < M_PI) {
            return std::sin(x);
        } else {
            return sgn(x) * 1.0f;
        }
    }
    float triodePoly(float x){
        return x - 0.2f*x*x + 0.05f*x*x*x;
    }

    float diode(float x){
        return std::tanh(2.5f * x);
    }

    float tape(float x){
        float a = 1.5f;
        return (1.f - std::exp(-a * x)) / (1.f - std::exp(-a));
    }
    float softtape(float x){
        float a = 1.5f;
        float ax = a * std::fabs(x);
        float y = (1.f - std::exp(-ax)) / (1.f - std::exp(-a));
        return std::copysign(y, x);
    }

    float foldback(float x){
        float t = 1.f;
        if (fabsf(x) > t)
            x = fabsf(fabsf(fmodf(x - t, t*4)) - t*2) - t;
        return x;
    }

    float softAbs(float x, float eps=float(1e-4)){
        return std::sqrt(x*x + eps);
    }
    float rectifier(float x){
        return softAbs(x);
    }


    float parabolic(float x){
        return x / (1.f + x*x);
    }

    inline float clamp1(float x){
        return std::clamp(x, float(-1), float(1));
    }

    float cheby2(float x){
        x = clamp1(x);
        return float(2)*x*x - float(1);
    }

    float cheby3(float x){
        x = clamp1(x);
        return float(4)*x*x*x - float(3)*x;
    }

    float cheby4(float x){
        x = clamp1(x);
        float x2 = x*x;
        return float(8)*x2*x2 - float(8)*x2 + float(1);
    }

    float cheby5(float x){
        x = clamp1(x);
        float x2 = x*x;
        float x3 = x2*x;
        return float(16)*x3*x2 - float(20)*x3 + float(5)*x;
    }

    float harmonicMix(float x, float a2, float a3, float a4, float a5){
        return a2*cheby2(x) + a3*cheby3(x) + a4*cheby4(x) + a5*cheby5(x);
    }

    float asymSCurve(float x){
        return (x >= 0.f) ? (1.f - std::exp(-x)) : -(1.f - std::exp(1.5f * x));
    }

    float powerDist(float x){
        return sgn(x) * powf(fabsf(x), 0.5f);
    }

    float softCrush(float x){
        return roundf(x * 8.f) / 8.f;
    }
    float softCrush2(float x){
        float q = std::round(x * 8.f) / 8.f;
        return std::tanh(float(2.0) * q);
    }

    float gainCompFromK(float k){
        // k ~ effective drive into nonlinearity
        // keeps level roughly stable as k increases
        return float(1) / std::sqrt(float(1) + float(0.5) * k*k);
    }

    float shapeGainComp(){
        switch (waveshape){
            case WS_HARDCLIP:   return 0.7f;
            case WS_FOLDBACK:   return 0.8f;
            case WS_RECTIFIER:  return 0.5f;
            case WS_CHEBY3:     return 0.6f;
            case WS_POWER:      return 0.8f;
            case WS_SOFTCRUSH:  return 0.9f;
            default:            return 1.0f;
        }
    }

    float shape(float x, float curve){
        x *= curve;

        switch (waveshape)
        {
            case WS_LOGICCLIP:  return logiclip(x);
            case WS_HARDCLIP:   return hardclip(x);
            case WS_TANCLIP:    return tanclip(x);
            case WS_QUINTIC:    return quintic(x);
            case WS_CUBASIC:    return cubicBasic(x);
            case WS_ALGCLIP:    return algClip(x);
            case WS_ARCCLIP:    return arcClip(x);
            case WS_SINCLIP:    return sinclip(x);
            case WS_SOFTEXP:    return std::copysign(1.f - std::exp(-fabsf(x)), x);

            case WS_TRIODE:     return std::tanh(float(1.2) * triodePoly(x));
            case WS_DIODE:      return diode(x);
            case WS_TAPE:       return softtape(x);
            case WS_FOLDBACK:   return foldback(x);
            case WS_RECTIFIER:  return rectifier(x);
            case WS_PARABOLIC:  return parabolic(x);
            case WS_CHEBY3:     return cheby3(x);
            case WS_ASYMSCURVE: return asymSCurve(x);
            case WS_POWER:      return powerDist(x);
            case WS_SOFTCRUSH:  return softCrush(x);
        }

        return x;
    }

    float dc_x1 = 0, dc_y1 = 0;

    inline float dcBlock(float x){
        float y = x - dc_x1 + float(0.995) * dc_y1;
        dc_x1 = x;
        dc_y1 = y;
        return y;
    }

};


