#pragma once
#include "Engine.hpp"
#include "Parameters.hpp"


struct ModFilter1 : public Function
{
    virtual float process(float input) = 0;

    void setCutoff(float f) { baseCutoff = f; }
    void setFM(float f)     { cutoff.set(f); }
    void setAM(float a)     { am.set(a); }

protected:
    float baseCutoff = 1000.0f;
    
    Modulator cutoff;
    Modulator am;

    template<class Filter>
    float update(Filter& filter, float input)
    {
        float _cut  = cutoff.process();   // [-1,1]        
        float _am   = am.process();       // [0,1] or similar

        float fc  = baseCutoff + baseCutoff * _cut;
     
        float sr = gam::sampleRate();
        fc = std::clamp(fc, 0.001f*sr, 0.45f*sr);
     
        filter.freq(fc);
        
        return filter(input) * _am;
    }
};

struct ModFilter2 : public Function
{
    virtual float process(float input) = 0;

    void setCutoff(float f) { baseCutoff = f; }
    void setWidth(float f)  { baseWidth = f; }
    void setFM(float f)     { cutoff.set(f); }
    void setWM(float w)     { width.set(w); }
    void setAM(float a)     { am.set(a); }

protected:
    float baseCutoff = 1000.0f;
    float baseWidth  = 0.0f;
    
    Modulator cutoff;
    Modulator width;
    Modulator am;

    template<class Filter>
    float update(Filter& filter, float input)
    {
        float _cut  = cutoff.process();   // [-1,1]
        float _wid  = width.process();
        float _am   = am.process();       // [0,1] or similar

        float fc  = baseCutoff + baseCutoff * _cut;
        float w   = baseWidth  + baseWidth  * _wid;

        fc = std::max(fc, 5.0f);          // safety
        
        filter.freq(fc);
        filter.width(w);
        return filter(input) * _am;
    }
};


struct ModFilter : public Function
{
    virtual float process(float input) = 0;

    void setCutoff(float f) { baseCutoff = f; }
    void setRes(float f)    { baseRes = f; }
    void setFM(float f)     { cutoff.set(f); }
    void setQM(float r)     { res.set(r); }
    void setAM(float a)     { am.set(a); }
    void setIM(float a)     { im.set(a); }
    void setFBM(float a)     { fb.set(a); }

protected:
    float baseCutoff = 1000.0f;
    float baseRes    = 0.707f;
    float last_y     = 0.0f;

    Modulator cutoff;
    Modulator res;
    Modulator am;
    Modulator im;    
    Modulator fb;

    template<class Filter>
    float update(Filter& filter, float input)
    {
        float _cut  = cutoff.process();   // [-1,1]
        float _res  = res.process();      // [-1,1]
        float _am   = am.process();       // [0,1] or similar
        float _im   = im.process();        
        float _fb   = fb.process();

        float fc  = baseCutoff + baseCutoff * _cut;
        float q   = baseRes    + baseRes    * _res;

        float sr = (float)gam::sampleRate();
        fc = std::clamp(fc, 0.001f * sr, 0.45f * sr);
        q  = std::max(q, 0.001f);

        filter.freq(fc);
        filter.res(q);

        float y = filter(_im * input + _fb * last_y) * _am;
        last_y = y;
        return y;
    }
};
