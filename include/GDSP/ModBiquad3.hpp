#pragma once
#include <algorithm>
#include "Biquad3.hpp"

class ModBiquad3 : public Function {
public:
    using FilterType = gam::FilterType;

    ModBiquad3(float f0, float f1, float f2,
               float Q = 1.0f,
               FilterType t0 = FilterType::LOW_SHELF,
               FilterType t1 = FilterType::PEAKING,
               FilterType t2 = FilterType::HIGH_SHELF,
               float level = 1.0f)
    : mFilter(f0,f1,f2,Q),
      last_y(0.0f)
    {
        mFilter.setType0(t0);
        mFilter.setType1(t1);
        mFilter.setType2(t2);
        mFilter.setLevel(level);

        baseF0 = f0;
        baseF1 = f1;
        baseF2 = f2;
        baseQ  = Q;

        am.set(1.0f);
        im.set(1.0f);        
        fb.set(0.0f);
    }

    // -------- Base parameter controls (slow knobs) --------

    void setCutoff0(float f) { baseF0 = f; }
    void setCutoff1(float f) { baseF1 = f; }
    void setCutoff2(float f) { baseF2 = f; }
    void setQ(float q)       { baseQ  = q; }

    void setType0(gam::FilterType type) { mFilter.setType0(type); }
    void setType1(gam::FilterType type) { mFilter.setType1(type); }
    void setType2(gam::FilterType type) { mFilter.setType2(type); }

    // -------- Modulation depths --------

    void setFM0(float v) { fm0.set(v); }
    void setFM1(float v) { fm1.set(v); }
    void setFM2(float v) { fm2.set(v); }
    void setQM(float v)  { qm.set(v);  }

    void setDepthF0(float v) { depthF0 = v; }
    void setDepthF1(float v) { depthF1 = v; }
    void setDepthF2(float v) { depthF2 = v; }

    void setAM(float v)  { am.set(v); }
    void setIM(float v)  { im.set(v); }    
    void setFB(float v)  { fb.set(v); }

    // -------- Audio processing --------

    float process(float input) override
    {
        const float _f0  = fm0.process();
        const float _f1  = fm1.process();
        const float _f2  = fm2.process();
        const float _q   = qm.process();
        const float _am  = am.process();
        const float _im  = im.process();        
        const float _fb  = fb.process();

        float f0 = baseF0 + depthF0 * baseF0 * _f0;
        float f1 = baseF1 + depthF1 * baseF1 * _f1;
        float f2 = baseF2 + depthF2 * baseF2 * _f2;

        float q  = baseQ  + (baseQ * depthQ * _q);

        float sr = (float)gam::sampleRate();

        f0 = std::clamp(f0, 5.0f, 0.45f * sr);
        f1 = std::clamp(f1, 5.0f, 0.45f * sr);
        f2 = std::clamp(f2, 5.0f, 0.45f * sr);
        q  = std::max(q, 0.001f);

        mFilter.setFreqs(f0, f1, f2);
        mFilter.setRes(q);

        float y = mFilter.process(_im * input + _fb * last_y);
        y *= _am;

        last_y = y;
        return y;
    }

private:
    Biquad3 mFilter;

    // Base parameters
    float baseF0 = 400.0f;
    float baseF1 = 1200.0f;
    float baseF2 = 2800.0f;
    float baseQ  = 1.0f;

    float depthF0 = 1.0f;
    float depthF1 = 1.0f;
    float depthF2 = 1.0f;
    float depthQ  = 1.0f;

    // Modulators
    Modulator fm0, fm1, fm2, qm;
    Modulator am, im, fb;

    float last_y;
};
