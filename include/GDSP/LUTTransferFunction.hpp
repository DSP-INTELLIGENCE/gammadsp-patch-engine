#pragma once
#include <array>
#include <cmath>
#include <algorithm>
#include <functional>
#include "Engine.hpp"
#include "Parameters.hpp"
#include "BlockDC.hpp"


class LUTTransferFunction : public Function
{
public:
    static constexpr int Size = 4096;

    enum TransferType {
        SOFTCLIP,
        HARDCLIP,
        FOLDBACK,
        SAT_EXP,
        SAT_LOG,

        // Diode
        DIODE_SOFT,
        DIODE_ASYM,
        DIODE_THRESH,
        DIODE_SHOCKLEY,

        // Tube
        TUBE_POLY,
        TUBE_TRIODE,
        TUBE_ASYM_S
    };


    LUTTransferFunction(TransferType type)
    {
        setType(type);
    }

    void setType(TransferType type)
    {
        if (currentType != type)
        {
            currentType = type;
            dirty = true;
        }
    }

    void setDrive(float v) { k = std::max(0.01f, v); dirty = true; }
    void setAsym(float p, float n) { kp = p; kn = n; dirty = true; }
    void setThreshold(float t) { T = Tp = Tn = t; dirty = true; }
    void setTubeABC(float a, float b, float k) {
        tubeA = a; tubeB = b; tubeK = k;
        dirty = true;
    }

    void setTubeAsym(float kp, float kn) {
        tubeKP = kp; tubeKN = kn;
        dirty = true;
    }

    float process(float x) override
    {
        if (dirty) rebuild();

        
        x = std::clamp(x, -1.0f, 1.0f);
        float t = (x + 1.0f) * 0.5f * (Size - 1);

        int i = (int)t;

        // ---- FIX: prevent table[i+1] OOB ----
        i = std::clamp(i, 0, Size - 2);

        float frac = t - (float)i;
        return table[i] + frac * (table[i + 1] - table[i]);
    }

    TransferType currentType = TransferType::SOFTCLIP;
    float k  = 4.0f;     // main drive
    float kp = 4.0f;
    float kn = 3.0f;
    float T  = 0.4f;
    float Tp = 0.4f;
    float Tn = 0.4f;
    float tubeA = 0.22f;
    float tubeB = 0.05f;
    float tubeK = 3.0f;
    float tubeKP = 3.5f;
    float tubeKN = 2.2f;
    

private:
    std::array<float, Size> table{};
    bool dirty = true;
    
    // Helpers
    inline float sgnf(float x) { return (0.f < x) - (x < 0.f); }
    inline float clampf(float x, float lo, float hi){ return std::max(lo, std::min(x, hi)); }

    // 1) Soft clip (normalized tanh)
    inline float softclip_tanh(float x, float k){
        float d = std::tanh(k);
        return (d > 1e-8f) ? std::tanh(k*x)/d : x;
    }

    // 2) Hard clip
    inline float hardclip(float x, float T){
        return clampf(x, -T, T) / T; // normalized to [-1,1]
    }

    // 3) Foldback (canonical)
    inline float foldback(float x, float T){
        float a = std::fabs(x);
        if(a <= T) return x / T; // normalize
        float p = std::fmod(a - T, 4.f*T);
        float y = T - std::fabs(p - 2.f*T);
        return sgnf(x) * (y / T);
    }

    // 4) Exponential saturator (canonical, normalized)
    inline float sat_exp(float x, float k){
        float ax = std::fabs(x);
        float num = 1.f - std::exp(-k * ax);
        float den = 1.f - std::exp(-k);
        float y = (den > 1e-8f) ? (num/den) : ax;
        return std::copysign(y, x);
    }

    // 5) Log saturator (canonical, normalized)
    inline float sat_log(float x, float k){
        float ax = std::fabs(x);
        float num = std::log1pf(k * ax);
        float den = std::log1pf(k);
        float y = (den > 1e-8f) ? (num/den) : ax;
        return std::copysign(y, x);
    }
    inline float diode_soft(float x, float k)
    {
        float ax = std::fabs(x);
        float num = 1.f - std::exp(-k * ax);
        float den = 1.f - std::exp(-k);
        float y = (den > 1e-8f) ? (num / den) : ax;
        return std::copysign(y, x);
    }
    inline float diode_asym(float x, float kp, float kn)
    {
        if(x >= 0.f)
            return 1.f - std::exp(-kp * x);
        else
            return -(1.f - std::exp(-kn * -x));
    }
    inline float diode_thresh(float x, float T, float k)
    {
        float a = std::fabs(x);
        if(a < T) return x;

        float y = T + (1.f - std::exp(-k * (a - T)));
        return std::copysign(y, x);
    }
    inline float diode_shockley(float x,
                                float Tp, float Tn,
                                float kp, float kn)
    {
        if(x >= Tp)
        {
            return Tp + (1.f - std::exp(-kp * (x - Tp)));
        }
        else if(x <= -Tn)
        {
            return -(Tn + (1.f - std::exp(-kn * (-x - Tn))));
        }
        else
        {
            return x;
        }
    }
    inline float tube_poly(float x, float a, float b, float k)
    {
        float u = x - a * x * x + b * x * x * x;
        return std::tanh(k * u);
    }
    inline float tube_triode(float x, float a, float b, float k)
    {
        float u = x - a * x * x + b * x * x * x;
        return std::tanh(k * u);
    }
    inline float tube_asym_s(float x, float kp, float kn)
    {
        if(x >= 0.f)
        {
            float d = 1.f - std::exp(-kp);
            return (d > 1e-8f) ? (1.f - std::exp(-kp * x)) / d : x;
        }
        else
        {
            float d = 1.f - std::exp(-kn);
            return (d > 1e-8f) ? -(1.f - std::exp(-kn * -x)) / d : x;
        }
    }

    void rebuild()
    {
        // ---- Choose canonical constants (tweak as desired) ----
        constexpr float kSoft = 3.5f;
        constexpr float THard = 1.0f;
        constexpr float TFold = 1.0f;
        constexpr float kExp  = 4.0f;
        constexpr float kLog  = 4.0f;

        for (int i = 0; i < Size; ++i)
        {
            float x = 2.0f * i / (Size - 1) - 1.0f;

            switch (currentType)
            {
                case TransferType::SOFTCLIP: table[i] = softclip_tanh(x, kSoft); break;
                case TransferType::HARDCLIP: table[i] = hardclip(x, THard);      break;
                case TransferType::FOLDBACK: table[i] = foldback(x, TFold);      break;
                case TransferType::SAT_EXP:  table[i] = sat_exp(x, kExp);        break;
                case TransferType::SAT_LOG:  table[i] = sat_log(x, kLog);        break;
                case TransferType::DIODE_SOFT:
                    table[i] = diode_soft(x, k);
                    break;

                case TransferType::DIODE_ASYM:
                    table[i] = diode_asym(x, kp, kn);
                    break;

                case TransferType::DIODE_THRESH:
                    table[i] = diode_thresh(x, T, k);
                    break;

                case TransferType::DIODE_SHOCKLEY:
                    table[i] = diode_shockley(x, Tp, Tn, kp, kn);
                    break;
                case TransferType::TUBE_POLY:
                    table[i] = tube_poly(x, tubeA, tubeB, tubeK);
                    break;

                case TransferType::TUBE_TRIODE:
                    table[i] = tube_triode(x, tubeA, tubeB, tubeK);
                    break;

                case TransferType::TUBE_ASYM_S:
                    table[i] = tube_asym_s(x, tubeKP, tubeKN);
                    break;

            }

            // ---- Normalize table to [-1, 1] ----
            float maxAbs = 0.f;
            for (float v : table)
                maxAbs = std::max(maxAbs, std::fabs(v));

            if (maxAbs > 1.5f)
            {
                float inv = 1.5f / maxAbs;
                for (float& v : table)
                    v *= inv;
            }

        }

        dirty = false;
    }
};
