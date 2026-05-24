#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif
#include <cassert>
#include <vector>
#include <memory>
#include <atomic>
#include "Engine.hpp"
#include "Parameters.hpp"

class LFO : public Generator {
public:
    enum Wave {
        SINE,
        SINE_PARA,
        COS,
        COS_CUBIC,
        SAW_DOWN,
        SAW_UP,
        TRIANGLE,
        SQUARE,
        PULSE,
        PARABOLA,
        STAIR,
        IMPULSE,
        HANN,
        EVEN3,
        EVEN5,
        LINE2,
        UP2,
        PATTERN,
        S1,
        C2,
        S3,
        C4,
        S5,
        COS_CUBU,
        DOWNU,
        IMPU,
        LINE2U,
        PARAU,
        STAIRU,
        TRIU,
        UPU,
        UP2U
    };

    LFO(float freq = 1.f,
        float phase = 0.f,
        float mod = 0.5f,
        Wave wave=SINE)
    : mLfo(freq, phase, mod), mWave(wave)
    {
        
    }

    void setFreq(float f)
    {
        mLfo.freq(f);
    }

    void setPeriod(float seconds)
    {
        if(seconds > 0.f)
            mLfo.freq(1.f / seconds);
    }

    void setPhase(float p)
    {
        mLfo.phase(p);
    }

    void setMod(float m)
    {
        mLfo.mod(m);
    }

    void setWave(Wave w)
    {
        mWave = w;
    }

    float tick()
    {
        switch(mWave)
        {
            case SINE:        return mLfo.sin();
            case SINE_PARA:   return mLfo.sinPara();
            case COS:         return mLfo.cos();
            case COS_CUBIC:   return mLfo.cosCub();
            case SAW_DOWN:    return mLfo.down();
            case SAW_UP:      return mLfo.up();
            case TRIANGLE:    return mLfo.tri();
            case SQUARE:      return mLfo.sqr();
            case PULSE:       return mLfo.pulse();
            case PARABOLA:    return mLfo.para();
            case STAIR:       return mLfo.stair();
            case IMPULSE:     return mLfo.imp();
            case HANN:        return mLfo.hann();
            case EVEN3:       return mLfo.even3();
            case EVEN5:       return mLfo.even5();
            case LINE2:       return mLfo.line2();
            case UP2:         return mLfo.up2();
            case PATTERN:     return mLfo.patU() * 2.f - 1.f;
            case S1:          return mLfo.S1();
            case C2:          return mLfo.C2();
            case S3:          return mLfo.S3();
            case C4:          return mLfo.C4();
            case S5:          return mLfo.S5();
            case COS_CUBU:    return mLfo.cosCubU();
            case DOWNU:       return mLfo.downU();
            case IMPU:        return mLfo.impU();
            case LINE2U:      return mLfo.line2U();
            case PARAU:       return mLfo.paraU();
            case STAIRU:      return mLfo.stairU();
            case TRIU:        return mLfo.triU();
            case UPU:         return mLfo.upU();
            case UP2U:        return mLfo.up2U();
            default:          return mLfo.sin();
        }
    }

    float process() override
    {
        float r = tick();
        if(bUnipolar) r = (1.0f + r)*0.5f;
        return r;
    }
    

    void reset()
    {
        mLfo.phase(0.f);
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

    void  setUnipolar(bool v) { bUnipolar = v; }
    void  phaseAdd(float p) { mLfo.phaseAdd(p); }
    void  freq(float p) { mLfo.freq(p);}
    float freq() { return mLfo.freq(); }

    float operator()() { return process(); }

private:
    gam::LFO<> mLfo;
    Wave mWave;
    bool bUnipolar = false;
    
};


