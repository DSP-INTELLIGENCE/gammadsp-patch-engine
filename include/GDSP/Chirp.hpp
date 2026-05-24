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

class Chirp : public Generator {
public:
    Chirp(float startFreq = 220.f,
          float endFreq   = 0.f,
          float decay60   = 0.2f)
    : 
    mStartFreq(startFreq),
    mEndFreq(endFreq),
    mDecay60(decay60),
    mChirp(startFreq, endFreq, decay60)
    {
        
    }

    void setFreqs(float startFreq, float endFreq)
    {
        mStartFreq = startFreq;
        mEndFreq   = endFreq;
        mChirp.freq(startFreq, endFreq);
    }

    void setDecay(float decay60)
    {
        mDecay60 = decay60;
        mChirp.decay(decay60);
    }

    void reset()
    {
        mChirp.reset();
    }

    float process()
    {
        return mChirp();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

    float mStartFreq;
    float mEndFreq;
    float mDecay60;
    
private:
    gam::Chirp<double> mChirp;    
};


