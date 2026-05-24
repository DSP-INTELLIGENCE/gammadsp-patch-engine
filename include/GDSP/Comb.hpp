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

class Comb : public Function {
public:
    Comb(float maxDelaySeconds  = 2.0f,
         float initDelaySeconds = 0.05f)
    :
    mComb(maxDelaySeconds, initDelaySeconds, 0.f, 0.f)
    {
    
    }
    
    void setDelay(float seconds) { mComb.delay(std::clamp(seconds, 0.f, getMaxDelayTime())); }
    void setFeedback(float fb) { mComb.fbk(std::clamp(fb, -0.999f, 0.999f)); }
    void setFeedforward(float ff) { mComb.ffd(std::clamp(ff, -1.f, 1.f)); }
    void setAllPass(float ap) { mComb.allPass(std::clamp(ap, -0.999f, 0.999f)); }
    void setDelayLength(float length) { mComb.delay(length); }
    void setDelaySamplesLength(uint32_t length) { mComb.delaySamples(length); }
    void setDelaySamplesLengthR(float length) { mComb.delaySamplesR(length); }
    void setFreq(float freq) { mComb.freq(freq); }
    void setIpolType(gam::ipl::Type type) { mComb.ipolType(static_cast<gam::ipl::Type>(type)); }
    void setMaxDelay(float delay, bool setDelay=true) { mComb.maxDelay(delay,setDelay); }    

    float getMaxDelayTime() const { return mComb.maxDelay(); }    
    float getDelayTime() { return mComb.delay(); }

    float read() const { return mComb.read(); }
    float read(float ago) const { return mComb.read(ago); }
    void write(const float& v) { mComb.write(v); }
    float feedback(float input) { return mComb.nextFbk(input); }
    float process(float input) { return mComb(input);  }

    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }

private:
    
    gam::Comb<double> mComb;
    
};
