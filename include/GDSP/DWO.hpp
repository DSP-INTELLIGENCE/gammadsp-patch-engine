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

class DWO : public Generator {
public:
    enum Wave {
        UP,
        DOWN,
        SAW = DOWN,
        SQUARE,
        PARABOLA,
        TRIANGLE,
        PULSE,
        IMPULSE
    };

    DWO(float freq = 440.f,
        float phase = 0.f,
        float mod = 0.5f)
    : mOsc(freq, phase, mod)
    {
        
    }

    float up() { return mOsc.up(); }
    float down() { return mOsc.down(); }
    float saw(){ return mOsc.saw(); } ///< Saw
    float sqr() { return mOsc.sqr(); }
    float para() { return mOsc.para(); }
    float tri() { return mOsc.tri(); }
    float pulse() { return mOsc.pulse(); }
    float imp() { return mOsc.imp(); }


    void setFreq(float f)
    {
        mOsc.freq(f);
    }

    void setPeriod(float seconds)
    {
        if(seconds > 0.f)
            mOsc.period(seconds);
    }

    void setPhase(float p)
    {
        mOsc.phase(p);
    }

    void setMod(float m)
    {
        mOsc.mod(m);
    }

    float process() override
    {
        switch(_type)
        {
            case UP:       return mOsc.up();
            case DOWN:     return mOsc.down();
            case SQUARE:   return mOsc.sqr();
            case PARABOLA: return mOsc.para();
            case TRIANGLE: return mOsc.tri();
            case PULSE:    return mOsc.pulse();
            case IMPULSE:  return mOsc.imp();
            default:       return 0.f;
        }
    }

    void reset()
    {
        mOsc.phase(0.f);
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }



    Wave _type = SAW;

private:
    gam::DWO<> mOsc;
};




