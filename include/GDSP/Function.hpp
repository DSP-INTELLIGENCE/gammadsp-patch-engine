// Functions.hpp
#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif
#include <cassert>
#include <cmath>
#include <complex>
#include <vector>
#include <memory>
#include <algorithm>


class FabsF : public Function {
public:
    float process(float in) override {
        return std::fabs(in);
    }
};

class ClampF : public Function {
public:
    float process(float in) override {
        return std::clamp(in,min,max);
    }

    float min = -1.0f;
    float max = 1.0f;
};

class AbsF : public Function {
public:
    float process(float in) override {
        return gam::scl::abs<float>(in);
    }
};


class DbToAmpF : public Function {
public:
    float process(float in) override {
        return gam::scl::dbToAmp<float>(in);
    }
};

class AmpToDbF : public Function {
public:
    float process(float in) override {
        return gam::scl::ampToDb<float>(in);
    }
};

class FloorF : public Function {
public:
    float process(float in) override {
        return std::floor(in);
    }
};

class CeilF : public Function {
public:
    float process(float in) override {
        return std::ceil(in);
    }
};


class ExpF : public Function {
public:
    float process(float in) override {
        return std::exp(in);
    }
};

class LogF : public Function {
public:
    float process(float in) override {
        return std::log(in);
    }
};

class Log10F : public Function {
public:
    float process(float in) override {
        return std::log10(in);
    }
};

class Log2F : public Function {
public:
    float process(float in) override {
        return std::log2(in);
    }
};



class TanhF : public Function {
public:
    float process(float in) override {
        return std::tanh(in);
    }
};

class SinhF : public Function {
public:
    float process(float in) override {
        return std::sinh(in);
    }
};

class CoshF : public Function {
public:
    float process(float in) override {
        return std::cosh(in);
    }
};


class SinF : public Function {
public:
    float process(float in) override {
        return std::sin(in);
    }
};

class CosF : public Function {
public:
    float process(float in) override {
        return std::cos(in);
    }
};


class ATanhF : public Function {
public:
    float process(float in) override {
        return std::atanh(in);
    }
};

class ASinhF : public Function {
public:
    float process(float in) override {
        return std::asinh(in);
    }
};

class ACoshF : public Function {
public:
    float process(float in) override {
        return std::acosh(in);
    }
};



class TanF: public Function {
public:

    float process(float in) override {
        return std::tan(in);
    }
};


class ASinF : public Function {
public:
    float process(float in) override {
        return std::asin(in);
    }
};

class ACosF : public Function {
public:
    float process(float in) override {
        return std::acos(in);
    }
};

class ATanF: public Function {
public:

    float process(float in) override {
        return std::atan(in);
    }
};

class SqrF : public Function {
public:
    float process(float in) override {
        return in*in;
    }
};


class SqrtF : public Function {
public:
    float process(float in) override {
        return std::sqrt(in);
    }
};

class EnvelopeFollower : public Function {
public:
    EnvelopeFollower(float attack = 0.01f, float release = 0.1f, float sr = 48000.f) {
        set(attack, release, sr);
    }

    void set(float attack, float release, float sr) {
        mA = std::exp(-1.f / (attack  * sr));
        mR = std::exp(-1.f / (release * sr));
    }

    float process(float in) override {
        float x = std::fabs(in);
        if (x > mEnv)  mEnv = mA * mEnv + (1.f - mA) * x;
        else          mEnv = mR * mEnv + (1.f - mR) * x;
        return mEnv;
    }

private:
    float mEnv = 0.f;
    float mA = 0.f, mR = 0.f;
};

class PeakHold : public Function {
public:
    PeakHold(size_t hold = 1024) : mHold(hold) {}

    float process(float in) override {
        float x = std::fabs(in);
        if (x > mPeak) { mPeak = x; mCounter = 0; }
        else if (++mCounter >= mHold) mPeak *= 0.9995f;
        return mPeak;
    }

private:
    float  mPeak = 0.f;
    size_t mCounter = 0, mHold;
};

class DCReject : public Function {
public:
    float process(float in) override {
        float y = in - mPrev + 0.995f * mOut;
        mPrev = in;
        mOut = y;
        return y;
    }

private:
    float mPrev = 0.f, mOut = 0.f;
};

class SoftClip : public Function {
public:
    float process(float in) override {
        return std::tanh(in);
    }
};

class TransientDetector : public Function {
public:
    float process(float in) override {
        float diff = std::fabs(in - mPrev);
        mPrev = in;
        return diff;
    }

private:
    float mPrev = 0.f;
};

class PowF : public Function {
public:
    
    float y = 1.0f;
    float process(float x) override {
        return std::pow(x,y);
    }
    float process(float x, float y) {
        return std::pow(x,y);
    }
};

class AddF : public Function {
public:
    
    float lastval = 0.0f;
    float process(float x) override {
        float t =  x + lastval;
        lastval = x;
        return t;
    }
    float process(float x, float y) {
        return x + y;
    }
};

class SubF : public Function {
public:
    
    float subf = 0.0f;
    float process(float x) override {
        float t =  x - lastval;
        lastval = x;
        return t;
    }
    float process(float x, float y) {
        return x - y;
    }
};

class MulF : public Function {
public:
    
    float lastval = 0.0f;
    float process(float x) override {
        float t =  x * lastval;
        lastval = x;
        return t;
    }
    float process(float x, float y) {
        return x * y;
    }
};

class DivF : public Function {
public:
    
    float lastval = 0.0f;
    float process(float x) override {
        float t =  x / lastval;
        lastval = x;
        return t;
    }
    float process(float x, float y) {
        return x / y;
    }
};