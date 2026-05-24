//////////////////////////////////////////////////////////
// Spatial.hpp
// Spatial/Reverb (true stereo late field)
//////////////////////////////////////////////////////////
#pragma once
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

class EchoF : public Function {
public:
    using EchoT = gam::Echo<float, gam::ipl::Linear, gam::Loop1P, gam::Domain1>;

    EchoF(float delay=0.25f, float decay=1.f, float damp=0.f)
    : mEcho(delay, decay, damp) {}

    void setDelay(float v)  { mDelay = std::max(0.f, v); }
    void setDecay(float v)  { mDecay = std::max(1e-6f, v); }
    void setDamping(float v){ mDamp  = std::clamp(v, -0.99999f, 0.99999f); }

    float process(float in) override {
        float d  = mDelay;
        float dc = mDecay;
        float dp = mDamp;

        mEcho.delay(d);
        mEcho.decay(dc);
        mEcho.damping(dp);
        return mEcho(in);
    }
    
    
private:
    EchoT mEcho;
    float mDelay=0.25f, mDecay=1.f, mDamp=0.f;
};

class Dist2 {
public:
    Dist2()
    : mDist(0.2f, 0.1f, 10.f)
    {
        mDist.blockSize(64);
        mDist.dist(0,1.f);
        mDist.dist(1,1.f);
    }

    void setLeftPos(float x,float y,float z=0){ mDist.dist(0,x,y,z); }
    void setRightPos(float x,float y,float z=0){ mDist.dist(1,x,y,z); }

    inline void process(float in, float& L, float& R){
        auto v = mDist(in);
        L = v[0];
        R = v[1];
    }

private:
    gam::Dist<2,float> mDist;
};

class ReverbMSF : public Function {
public:
    using RevT = gam::ReverbMS<float, gam::Loop1P, gam::ipl::Trunc, gam::Domain1>;

    ReverbMSF(gam::ReverbFlavor flavor=gam::FREEVERB)
    {
        mRev.resize(flavor);   // init-time only
        mRev.decay(1.f);
        mRev.damping(0.2f);
    }

    float process(float in) override {
        float d = mDecay;
        float w = mDamp;
        mRev.decay(std::max(1e-6f,d));
        mRev.damping(std::clamp(w, -0.999f, 0.999f));
        return mRev(in);
    }

private:
    RevT mRev;
    float mDecay=1.f, mDamp=0.2f;
};


