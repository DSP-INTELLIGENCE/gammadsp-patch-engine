//////////////////////////////////////////////////////////
// Engine
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
#include <complex>


struct StereoSample {
    float outL;
    float outR;

    StereoSample() : outL(0.0f),outR(0.0f) {}
    StereoSample(float L, float R) : outL(L), outR(R) {
        
    }
};

struct ComplexSample {
    float real;
    float imag;

    ComplexSample() : real(0.0f),imag(0.0f) {}
    ComplexSample(float r, float i): real(r), imag(i) {

    }
};

namespace gam {
    using DefaultDomain = DomainObserver;
}

template<class Td = gam::DefaultDomain>
struct Module : public Td {
    Module() = default;
    virtual ~Module() = default;

    /// Called when the domain sample rate changes
    /// ratioSPU = new_spu / old_spu
    virtual void onDomainChange(double ratioSPU){
        Td::onDomainChange(ratioSPU);        
    }

    
};



class Function: public Module<>
{
public:
    
    virtual ~Function() {}           
    virtual float process(float input) = 0;       

    float operator()(float input) { return process(input); } 

};

class StereoFunction: public Function
{
public:

    virtual ~StereoFunction() {}           
    virtual StereoSample process(const StereoSample& input) = 0;       
    
    StereoSample operator()(const StereoSample& input) { return process(input); } 

    float process(float input) {
        return process(StereoSample(input,input)).outL;
    }
};

class ComplexFunction: public Function
{
public:

    virtual ~ComplexFunction() {}           
    virtual ComplexSample process(const ComplexSample& input) = 0;        

    float process(float input) {
        return process(ComplexSample(input,0)).real;
    }
};

class Function2: public Function
{
public:

    float a,b;

    virtual ~Function2() {}           
    virtual float process(float input1, float input2) = 0;        

    float process(float input) {        
        a = input;
        return process(a,b);
    }
    float process() {
        return process(a,b);
    }
};

class FunctionN: public Function
{
public:
    virtual ~FunctionN() {}
    virtual float process(const float * input, size_t n) = 0;

    float process(float input) {
        return process(&input,1);
    }
};



class Generator: public Function
{
public:

    virtual ~Generator () {}    
    virtual float process() = 0;    

    float operator()() { return process(); }

    float process(float input) {
        return process();
    }
};

class ComplexGenerator: public Function
{
public:

    virtual ~ComplexGenerator () {}    
    virtual ComplexSample process() = 0;    
    
    float process(float input) {
        return process().real;
    }
};

class StereoGenerator: public Function
{
public:

    virtual ~StereoGenerator () {}    
    virtual StereoSample process() = 0;    
    
    float process(float input) {
        return process().outL;
    }
};

// constant/variable 
struct Value : public Generator
{
    float value;

    Value(float v = 0.0f): value(v) {}

    void set_value(float v) { value = v; }

    float process()  override { return value; }
};


// parallel sum
class FunctionBlock: public Function
{
private:

    std::vector<Function*> nodes;

public:

    FunctionBlock() {

    }
    virtual ~FunctionBlock() {

    }

    void addFilter(Function * dsp)
    {
        nodes.push_back(dsp);
    }

    float process(float input)
    {
        float sum = 0.0f;
        for (auto& f : nodes)
            sum += f->process(input);

        float gain = 1.0f / std::sqrt(std::max(1, (int)nodes.size()));
        return sum * gain;

    }
};

// serial filter -> filter -> chain
class FunctionChain : public Function
{
private:
    std::vector<Function*> nodes;

public:

    FunctionChain() {}
    virtual ~FunctionChain() {}

    Function& operator[](size_t i) {
        assert(i < nodes.size());
        return *nodes[i];
    }
    void addFilter(Function * node) {
        nodes.push_back(node);
    }
    float process(float input) {        
        assert(nodes.size() > 0);
        float r = nodes[0]->process(input);
        for(size_t i = 1; i < nodes.size(); i++)
            r = nodes[i]->process(r);
        return r;
    }
    size_t size() { return nodes.size(); }
};



