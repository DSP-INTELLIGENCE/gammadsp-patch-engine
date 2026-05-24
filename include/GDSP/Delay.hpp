// Delay.hpp
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

class Delay : public Function {
public:
    Delay(float maxDelaySeconds = 2.0f,
          float initDelay = 0.25f)
    : mDelay(maxDelaySeconds, initDelay)
    {        
    }
   
    void setDelay(float seconds) { mDelay.delay(std::clamp(seconds, 0.f, getMaxDelayTime())); }
    void setDelayLength(float length) { mDelay.delay(length); }
    void setDelaySamplesLength(uint32_t length) { mDelay.delaySamples(length); }
    void setDelaySamplesLengthR(float length) { mDelay.delaySamplesR(length); }
    void setFreq(float freq) { mDelay.freq(freq); }
    void setIpolType(gam::ipl::Type type) { mDelay.ipolType(static_cast<gam::ipl::Type>(type)); }
    void setMaxDelay(float delay, bool setDelay) { mDelay.maxDelay(delay,setDelay); }    

    float read() const { return mDelay.read(); }
    float read(float ago) const { return mDelay.read(ago); }
    void  write(const float& v) { mDelay.write(v); }

    float getDelayTime() const { return mDelay.delay(); }
    float getMaxDelayTime() const { return mDelay.maxDelay(); }
    float getFreq() const { return mDelay.freq(); }
    float process(float input) { return mDelay(input); }

    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }


private:
    gam::Delay<double,ipl::Switchable> mDelay;    
};

