//////////////////////////////////////////////////////////
// Parameters.hpp
// Parameters
//////////////////////////////////////////////////////////
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

class Ramped : public Generator {
public:
    Ramped(float initValue = 0.0f)
    : mRamp(initValue)
    {
    }

    void set(float value)
    {
        mRamp = value;
    }

    void target(float value, unsigned steps)
    {
        if(steps <= 1)
            mRamp = value;
        else
            mRamp.target(value, float(steps));
    }

    float value() const
    {
        return float(mRamp);
    }

    float target() const
    {
        return mRamp.target();
    }

    float process() override
    {
        return mRamp();
    }
    float operator()() { return process(); }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

    void targetMs(float value, float timeMs)
    {
        float sampleRate = gam::sampleRate();
        if(timeMs <= 0.0f || sampleRate <= 0.0f)
        {
            mRamp = value;
            return;
        }

        unsigned steps = static_cast<unsigned>(
            std::round(timeMs * 0.001f * sampleRate)
        );

        target(value, std::max(steps, 1u));
    }

private:
    gam::Ramped<float> mRamp;
};




class Smoother
{
public:

    Smoother() {}
    virtual ~Smoother() {}

    virtual void set(float v) = 0;
    virtual float process()   = 0;
    virtual void setTime(float ms) = 0;

    float operator()() { return process(); }
};


class ParamLinear: public Smoother {
public:
    ParamLinear(float initial = 0.f, float ms = 15.0f)
    : ramp(initial)
    {
        timeMs = ms;
    }

    void set(float v) {
        ramp.targetMs(v, timeMs);
        mRamping = true;
    }

    float process() {
        float v = ramp.process();
        
        if (mRamping && std::abs(v - ramp.target()) < 1e-6f)
            mRamping = false;

        return v;
    }

    
    void setTime(float ms) { timeMs = ms; }

    bool isRamping() const { return mRamping; }

    float value() const { return ramp.value(); }

private:
    Ramped ramp;
    float timeMs = 15.0f;
    bool  mRamping = false;    
};


class ParamSlew: public Smoother  {
public:
    ParamSlew(float initial = 0.f, float timeMs = 15.f)
    : value(initial), target(initial)
    {
        setTime(timeMs);
    }

    void set(float v)  {
        target = v;
    }

    float process() {
        float delta = target - value;

        if (delta >  maxStep) value += maxStep;
        else if (delta < -maxStep) value -= maxStep;
        else value = target;

        return value;
    }
    
    void setTime(float ms) {
        // max change per sample, domain-correct
        float samples = std::max(1.f, ms * 0.001f * (float)gam::sampleRate());
        maxStep = 1.f / samples;
    }

private:
    float value = 0.f;
    float target = 0.f;
    float maxStep = 0.f;
};




struct Parameter
{
    float   _value = 0.0f;
    Smoother * _smoother = nullptr;
    Parameter() : _value(0.0f) {}
    Parameter(const float v) : _value(v) {}
    virtual ~Parameter() {}

    virtual void set(float v) { _value = v; if(_smoother) _smoother->set(v); }
    Parameter& operator = (float v) { set(v); return *this; }
    float operator()() { return process(); }
    float operator()(float v) { set(v); return process(); }
    float process() { if(_smoother) return _smoother->process(); return _value; }

    void attachSmoother(Smoother * p) { _smoother = p; }
};

struct Modulator
{
    float       modIn     = 0.0f;
    float       modAmount = 1.0f;
    float       modScale  = 1.0f;
    float       modMin    = 0.0f;
    float       modMax    = 1.0f;    
    float       lastVal   = 0.0f;
    bool        bUnipolar = false;
    Generator  *_gen      = nullptr;

    Modulator() {
        
    }
    virtual ~Modulator() = default;

    float operator()()  { return process(); }
    float operator()(float v)  { return process(v); }

    void set(float v) { modIn = v; }
    Modulator& operator = (float v) { set(v); return *this; }
    virtual float process() { return mod(); }
    virtual float process(float input) { 
        float r = mod(); 
        if(bUnipolar) {
            return input * r;
        }
        else
        {
            return input + input * r;
        }
    }
    
    float mod()
    {
        if(_gen != nullptr) modIn = _gen->process();
        float x = modIn;                
        //x = std::clamp(x,modMin,modMax);
        lastVal = x;
        return x * modAmount * modScale;
    }        

    void attachGen(Generator * p) { _gen = p; }
};

