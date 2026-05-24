#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif
#include <cassert>
#include <vector>
#include <memory>
#include <atomic>
#include <array>
#include <cmath>
#include "Engine.hpp"
#include "Parameters.hpp"

class MultitapDelay : public Function {
public:
    MultitapDelay(float maxDelaySeconds,
                  unsigned numTaps,
                  float initDelaySeconds = 0.25f)
    : mDelay(maxDelaySeconds, numTaps)  
    {
        
    }

    void setTapTime(unsigned tap, float seconds) { mDelay.delay(std::clamp(seconds, 0.f, mDelay.maxDelay()), tap); }
    void setTapFreq(unsigned tap, float freqHz)  { mDelay.freq(freqHz, tap); }
    void setMaxDelay(float delay, bool setDelay) { mDelay.maxDelay(delay, setDelay); }
    void setIpolType(gam::ipl::Type type) { mDelay.ipolType(static_cast<gam::ipl::Type>(type)); }
    void setNumTaps(unsigned numTaps) { mDelay.taps(numTaps);  }

    float read(unsigned tap) const { return mDelay.read(tap); }
    void write(const float& v) { mDelay.write(v); }

    float process(float input) { return mDelay(input); }

    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }

private:
    gam::Multitap<double> mDelay;    
};

