// FaustMeta.hpp

#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>
#include "GammaDSP.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"


#define FAUSTFLOAT float
#define RESTRICT



struct Meta { 
    void declare(const char*, const char*) {}
};

struct UI {
    void openHorizontalBox(const char*) {}
    void openVerticalBox(const char*) {}
    void closeBox() {}
    void addVerticalSlider(const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) {}
    void addHorizontalSlider(const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) {}
};

class dsp {
public:
    virtual ~dsp() {}
    virtual int getNumInputs() = 0;
    virtual int getNumOutputs() = 0;
    virtual void init(int) = 0;
    virtual void compute(int, FAUSTFLOAT**, FAUSTFLOAT**) = 0;
};

class FaustFunction : public Function {
public:
    explicit FaustFunction(dsp* dsp) : mDSP(dsp) {
        mInPtrs[0]  = &mIn;
        mOutPtrs[0] = &mOut;
    }

    float process(float input) override {
        mIn = (FAUSTFLOAT)input;
        mDSP->compute(1, mInPtrs, mOutPtrs);
        return (float)mOut;
    }

    void run(FAUSTFLOAT* input, FAUSTFLOAT* output, int n) {
        mInPtrs[0]  = input;
        mOutPtrs[0] = output;
        mDSP->compute(n, mInPtrs, mOutPtrs);

        // restore single-sample pointers
        mInPtrs[0]  = &mIn;
        mOutPtrs[0] = &mOut;
    }

    dsp& faust() { return *mDSP; }

private:
    dsp* mDSP;   // non-owning; Engine must own this

    FAUSTFLOAT mIn  = 0;
    FAUSTFLOAT mOut = 0;

    FAUSTFLOAT* mInPtrs[1];
    FAUSTFLOAT* mOutPtrs[1];
};


template<class FaustDSP>
class FaustFunctionT : public Function {
public:
    explicit FaustFunctionT(FaustDSP* p) : mDSP(p) {
        mInPtrs[0]  = &mIn;
        mOutPtrs[0] = &mOut;
    }

    float process(float input) override {
        mIn = (FAUSTFLOAT)input;
        mDSP->compute(1, mInPtrs, mOutPtrs);
        return (float)mOut;
    }

    void processBlock(FAUSTFLOAT* input, FAUSTFLOAT* output, int n) {
        mInPtrs[0]  = input;
        mOutPtrs[0] = output;
        mDSP->compute(n, mInPtrs, mOutPtrs);

        // restore single-sample pointers
        mInPtrs[0]  = &mIn;
        mOutPtrs[0] = &mOut;
    }

    FaustDSP& faust() { return *mDSP; }

private:
    FaustDSP* mDSP;                 // non-owning, Engine-owned

    FAUSTFLOAT mIn  = 0;
    FAUSTFLOAT mOut = 0;
    FAUSTFLOAT* mInPtrs[1];
    FAUSTFLOAT* mOutPtrs[1];
};
