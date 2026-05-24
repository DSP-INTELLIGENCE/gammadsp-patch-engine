// PolyBlep.hpp
#pragma once
#include <cmath>
#include <algorithm>
#include "Engine.hpp"
#include "Parameters.hpp"

class PolyBLEP: public Generator {
public:
    enum Wave { SINE, SAW, SQUARE, TRI, PULSE, RECT };

    explicit PolyBLEP(double sr = 48000.0);

    void setSampleRate(double sr);
    void setFrequency(double hz);
    void setWave(Wave w);
    void setDuty(double d);
    void sync(double phase01);


    // One sample
    float process();

    void setSyncMaster(PolyBLEP* m) { syncMaster = m; }

    // 0.0 = off, 1.0 = hard sync, ~0.05–0.3 = soft sync sweet spot
    void setSyncAmount(double a) { syncAmount = std::clamp(a, 0.0, 1.0); }

    Wave   wave = SINE;
    PolyBLEP* syncMaster = nullptr;

    double sampleRate = 48000.0;
    double baseHz = 440.0;
    
    
    // state
    double phase = 0.0;      // cycles [0..1)
    double dp = 440.0 / 48000.0; // cycles/sample (instantaneous)

    // shape
    double duty = 0.5;
    bool wrappedLastTick = false;
    // 0 = no sync, 1 = hard sync, between = soft
    double syncAmount = 0.0;

    // --- feedback state ---
    double lastOut = 0.0;          // previous output sample (for feedback)
    double lastOutFold = 0.0;          // previous output sample (for feedback)
   
    void setFmLin(float v) { pFmLinHz.set(v); }
    void setFmExp(float v) { pFmExpOct.set(v); }
    void setPM(float v) { pPmRad.set(v*M_PI); }
    void setAM(float v) { pAm.set(v); }
    void setFbFM(float v) { pFbHz.set(v); }
    void setFbPM(float v) { pFbPmRad.set(v*M_PI); }
    void setFbPMAmt(float v) { pFbPmAmt.set(v); }
    void setPdAmt(float v) { pPdAmt.set(v); }
    void setFoldAmt(float v) { pFoldAmt.set(v); }
    void setFoldDrv(float v) { pFoldDrv.set(v); }

    // modulators (non-owning)
    Modulator pFmLinHz;
    Modulator pFmExpOct;
    Modulator pPmRad;
    Modulator pAm;
    
    // --- new modulators ---
    Modulator pFbHz;
    Modulator pFbPmRad;
    Modulator pFbPmAmt;
    Modulator pPdAmt;
    Modulator pFoldAmt;
    Modulator pFoldDrv;

private:

    static inline double wrap(double x) {
        x -= std::floor(x);
        return x;
    }

    static inline double blep(double t, double dt) {
        if (t < dt) {
            t /= dt;
            return t + t - t*t - 1.0;           // 2t - t^2 - 1
        }
        if (t > 1.0 - dt) {
            t = (t - 1.0) / dt;
            return t*t + t + t + 1.0;           // t^2 + 2t + 1
        }
        return 0.0;
    }

    static inline double blamp(double t, double dt) {
        // Integrated BLEP for slope discontinuities
        if (t < dt) {
            t /= dt;
            return t*t*t/3.0 - t*t/2.0 + t/6.0 - 1.0/6.0;
        }
        if (t > 1.0 - dt) {
            t = (t - 1.0) / dt;
            return t*t*t/3.0 + t*t/2.0 + t/6.0 + 1.0/6.0;
        }
        return 0.0;
    }

    double eval(double p) const;

    double sine(double p)   const;
    double saw(double p)    const;
    double square(double p) const;
    double tri(double p)    const;
    double pulse(double p)  const;
    double rect(double p)   const;
};
