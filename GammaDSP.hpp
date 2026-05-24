// GammaDSP.hpp
#pragma once
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#include <cassert>
#include <vector>
#include <memory>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <algorithm>  // <-- add
#include <random>

extern "C" 
{
    void set_samplerate(double sampleRate);
    double get_samplerate();

    void deinterleave(float* interleaved,
                            float* left,
                            float* right,
                            int frames);

    void interleave(float* left,
                        float* right,
                        float* interleaved,
                        int frames);

    void deinterleaveN(float* interleaved,
                            float** planar,
                            int channels,
                            int frames);

    void interleaveN(float** planar,
                            float* interleaved,
                            int channels,
                            int frames);

    void swapStereo(float* interleaved, int frames);

}


struct StereoSample {
    float L;
    float R;

    StereoSample() : L(0.0f),R(0.0f) {}
    StereoSample(float _L, float _R) : L(_L), R(_R) {
        
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
    virtual ~Module() = default;

    /// Called when the domain sample rate changes
    /// ratioSPU = new_spu / old_spu
    virtual void onDomainChange(double ratioSPU){
        Td::onDomainChange(ratioSPU);
    }

    //double sampleRate() { return gam::sampleRate(); }
    //double spu() const;				///< Get samples/unit
	//double ups() const;				///< Get units/sample
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
        return process(StereoSample(input,input)).L;
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

    virtual ~Function2() {}           
    virtual float process(float input1, float input2) = 0;        

    float process(float input) {                
        return process(input, 0);
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
        return process().L;
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

class FunctionGraph : public Function
{
public:
    static constexpr int INPUT = -1;

    struct Edge
    {
        int   src  = INPUT;  // -1 means external input
        float gain = 1.0f;
    };

    struct Node
    {
        Function* fn = nullptr;          // non-owning
        std::vector<Edge> inputs;        // summed inputs
        float gain = 1.0f;
        bool  active = true;
    };

    FunctionGraph() = default;
    virtual ~FunctionGraph() = default;

    // Returns node index, or -1 on failure.
    int addNode(Function* fn)
    {
        if (!fn)
            return -1;

        Node n;
        n.fn = fn;

        nodes.push_back(n);
        values.resize(nodes.size(), 0.0f);

        return static_cast<int>(nodes.size() - 1);
    }

    bool connectInput(int dst, float gain = 1.0f)
    {
        if (!validNode(dst))
            return false;

        nodes[dst].inputs.push_back({ INPUT, gain });
        return true;
    }

    // Feed-forward connection only.
    // src must be before dst in processing order.
    bool connect(int src, int dst, float gain = 1.0f)
    {
        if (!validNode(src) || !validNode(dst))
            return false;

        // v1 graph is feed-forward only.
        // Feedback should be explicit one-sample feedback in a later graph.
        if (src >= dst)
            return false;

        nodes[dst].inputs.push_back({ src, gain });
        return true;
    }

    bool setNodeGain(int node, float gain)
    {
        if (!validNode(node))
            return false;

        nodes[node].gain = gain;
        return true;
    }

    bool setNodeActive(int node, bool active)
    {
        if (!validNode(node))
            return false;

        nodes[node].active = active;
        return true;
    }

    void clearInputs(int node)
    {
        if (validNode(node))
            nodes[node].inputs.clear();
    }

    bool addOutput(int src, float gain = 1.0f)
    {
        if (!validNode(src))
            return false;

        outputs.push_back({ src, gain });
        return true;
    }

    bool setOutput(int src, float gain = 1.0f)
    {
        if (!validNode(src))
            return false;

        outputs.clear();
        outputs.push_back({ src, gain });
        return true;
    }

    void clearOutputs()
    {
        outputs.clear();
    }

    void setNormalizeOutputs(bool v)
    {
        normalizeOutputs = v;
    }

    void setUnconnectedReadsInput(bool v)
    {
        unconnectedReadsInput = v;
    }

    size_t size() const
    {
        return nodes.size();
    }

    void clear()
    {
        nodes.clear();
        values.clear();
        outputs.clear();
    }

    void clearState()
    {
        std::fill(values.begin(), values.end(), 0.0f);
    }

    float process(float input) override
    {
        if (nodes.empty())
            return input;

        for (size_t i = 0; i < nodes.size(); ++i)
        {
            Node& n = nodes[i];

            float x = 0.0f;

            if (n.inputs.empty() && unconnectedReadsInput)
            {
                x = input;
            }
            else
            {
                for (const Edge& e : n.inputs)
                {
                    if (e.src == INPUT)
                    {
                        x += input * e.gain;
                    }
                    else if (e.src >= 0 && static_cast<size_t>(e.src) < values.size())
                    {
                        x += values[static_cast<size_t>(e.src)] * e.gain;
                    }
                }
            }

            if (n.active && n.fn)
                values[i] = n.fn->process(x) * n.gain;
            else
                values[i] = x;
        }

        // If no explicit graph outputs are set, output the last node.
        if (outputs.empty())
            return values.back();

        float y = 0.0f;

        for (const Edge& e : outputs)
        {
            if (e.src == INPUT)
            {
                y += input * e.gain;
            }
            else if (e.src >= 0 && static_cast<size_t>(e.src) < values.size())
            {
                y += values[static_cast<size_t>(e.src)] * e.gain;
            }
        }

        if (normalizeOutputs && outputs.size() > 1)
            y *= 1.0f / std::sqrt(static_cast<float>(outputs.size()));

        return y;
    }

    void run(const float* input, float* output, size_t n)
    {
        if (!input || !output)
            return;

        for (size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    bool validNode(int i) const
    {
        return i >= 0 && static_cast<size_t>(i) < nodes.size();
    }

private:
    std::vector<Node> nodes;
    std::vector<float> values;
    std::vector<Edge> outputs;

    bool normalizeOutputs = false;
    bool unconnectedReadsInput = true;
};

class DSPGraph : public Generator
{
public:
    static constexpr int INPUT = -1;

    enum class NodeType
    {
        Function,
        Generator
    };

    struct Edge
    {
        int   src  = INPUT;
        float gain = 1.0f;
    };

    struct Node
    {
        NodeType type = NodeType::Function;

        Function*  fn  = nullptr;   // non-owning
        Generator* gen = nullptr;   // non-owning, only valid for Generator nodes

        std::vector<Edge> inputs;

        float gain = 1.0f;
        bool  active = true;
    };

    DSPGraph() = default;
    virtual ~DSPGraph() = default;

    int addFunction(Function* fn)
    {
        if (!fn)
            return -1;

        Node n;
        n.type = NodeType::Function;
        n.fn   = fn;

        nodes.push_back(n);
        values.resize(nodes.size(), 0.0f);

        return static_cast<int>(nodes.size() - 1);
    }

    int addGenerator(Generator* gen)
    {
        if (!gen)
            return -1;

        Node n;
        n.type = NodeType::Generator;
        n.fn   = gen;
        n.gen  = gen;

        nodes.push_back(n);
        values.resize(nodes.size(), 0.0f);

        return static_cast<int>(nodes.size() - 1);
    }

    // Convenience fallback.
    // Generators can still enter as Function* because Generator inherits Function.
    int addNode(Function* fn)
    {
        if (!fn)
            return -1;

        Node n;
        n.type = NodeType::Function;
        n.fn   = fn;

        nodes.push_back(n);
        values.resize(nodes.size(), 0.0f);

        return static_cast<int>(nodes.size() - 1);
    }

    bool connectInput(int dst, float gain = 1.0f)
    {
        if (!validNode(dst))
            return false;

        nodes[dst].inputs.push_back({ INPUT, gain });
        return true;
    }

    bool connect(int src, int dst, float gain = 1.0f)
    {
        if (!validNode(src) || !validNode(dst))
            return false;

        // v1 graph is feed-forward only.
        // Feedback can be added later with explicit one-sample delay edges.
        if (src >= dst)
            return false;

        nodes[dst].inputs.push_back({ src, gain });
        return true;
    }

    bool addOutput(int src, float gain = 1.0f)
    {
        if (!validNode(src))
            return false;

        outputs.push_back({ src, gain });
        return true;
    }

    bool setOutput(int src, float gain = 1.0f)
    {
        if (!validNode(src))
            return false;

        outputs.clear();
        outputs.push_back({ src, gain });
        return true;
    }

    void clearOutputs()
    {
        outputs.clear();
    }

    bool setNodeGain(int node, float gain)
    {
        if (!validNode(node))
            return false;

        nodes[node].gain = gain;
        return true;
    }

    bool setNodeActive(int node, bool active)
    {
        if (!validNode(node))
            return false;

        nodes[node].active = active;
        return true;
    }

    void clear()
    {
        nodes.clear();
        values.clear();
        outputs.clear();
    }

    size_t size() const
    {
        return nodes.size();
    }

    void setNormalizeOutputs(bool v)
    {
        normalizeOutputs = v;
    }

    void setUnconnectedReadsInput(bool v)
    {
        unconnectedReadsInput = v;
    }

    // Generator mode.
    // Lets Python call: dsp.run(graph, output, N)
    float process() override
    {
        return process(0.0f);
    }

    // Function mode.
    // Lets Python call: dsp.run(graph, input, output, N)
    float process(float input) override
    {
        if (nodes.empty())
            return input;

        for (size_t i = 0; i < nodes.size(); ++i)
        {
            Node& n = nodes[i];

            float x = 0.0f;

            if (n.type == NodeType::Function)
            {
                if (n.inputs.empty() && unconnectedReadsInput)
                {
                    x = input;
                }
                else
                {
                    for (const Edge& e : n.inputs)
                    {
                        if (e.src == INPUT)
                        {
                            x += input * e.gain;
                        }
                        else if (e.src >= 0 && static_cast<size_t>(e.src) < values.size())
                        {
                            x += values[static_cast<size_t>(e.src)] * e.gain;
                        }
                    }
                }

                if (n.active && n.fn)
                    values[i] = n.fn->process(x) * n.gain;
                else
                    values[i] = x;
            }
            else
            {
                // Generator node ignores graph input and produces its own signal.
                if (n.active && n.gen)
                    values[i] = n.gen->process() * n.gain;
                else
                    values[i] = 0.0f;
            }
        }

        if (outputs.empty())
            return values.back();

        float y = 0.0f;

        for (const Edge& e : outputs)
        {
            if (e.src == INPUT)
            {
                y += input * e.gain;
            }
            else if (e.src >= 0 && static_cast<size_t>(e.src) < values.size())
            {
                y += values[static_cast<size_t>(e.src)] * e.gain;
            }
        }

        if (normalizeOutputs && outputs.size() > 1)
            y *= 1.0f / std::sqrt(static_cast<float>(outputs.size()));

        return y;
    }

private:
    bool validNode(int i) const
    {
        return i >= 0 && static_cast<size_t>(i) < nodes.size();
    }

private:
    std::vector<Node> nodes;
    std::vector<float> values;
    std::vector<Edge> outputs;

    bool normalizeOutputs = false;
    bool unconnectedReadsInput = true;
};

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


class AccumPhase : public Generator {
public:
    AccumPhase(float freq = 440.f,
               float phase = 0.f,
               float radius = 3.14159265358979323846f)
    : mAccum(freq, phase, radius)
    {}

    float nextPhase() { return mAccum.nextPhase(); }
    float nextPhase(float frqOffset) { return mAccum.nextPhase(frqOffset); }

    void freqAdd(float v) { mAccum.freqAdd(v); }
    void freqMul(float v) { mAccum.freqMul(v); }
    void phaseAdd(float v) { mAccum.phaseAdd(v); }

    float freqUnit() const { return mAccum.freqUnit(); }

    void setFreq(float f) { mAccum.freq(std::max(0.0f, f)); } // <-- change

    void setPeriod(float seconds) {
        if(seconds > 0.f) mAccum.period(seconds);
    }

    void setPhase(float p) { mAccum.phase(p); }
    void addPhase(float d) { mAccum.phaseAdd(d); }
    void setRadius(float r) { mAccum.radius(r); }

    float freq() const { return mAccum.freq(); }
    float period() const { return mAccum.period(); }
    float phase() const { return mAccum.phase(); }
    float radius() const { return mAccum.radius(); }

    float process() override { return mAccum.nextPhase(); }
    float process(float freqOffset) { return mAccum.nextPhase(freqOffset); }
    
    void run(float* output, size_t n) {
        for(size_t i = 0; i < n; ++i) output[i] = process();
    }

    void freq(float v) { mAccum.freq(std::max(0.0f, v)); } // <-- change

    float operator()() { return mAccum.nextPhase(); }
    float operator()(float v) { return mAccum.nextPhase(v); }

private:
    gam::AccumPhase<double> mAccum;
};

class AD : public Generator {
public:
    AD(float attack = 0.01f,
       float decay  = 0.1f,
       float level  = 1.0f,
       float curve  = -4.0f)
    : mAttack(attack),
    mDecay(decay),
    mLevel(level),
    mCurve(curve)
    {        
        mEnv.attack(mAttack);
        mEnv.decay(mDecay);
        mEnv.amp(mLevel);
        mEnv.curve(mCurve);

        mEnv.reset();
    }

    void setAttack(float seconds)
    {
        mAttack = std::max(0.f, seconds);
        mEnv.attack(mAttack);
    }

    void setDecay(float seconds)
    {
        mDecay = std::max(0.f, seconds);
        mEnv.decay(mDecay);
    }

    void setLevel(float level)
    {
        mLevel = std::max(0.f, level);
        mEnv.amp(mLevel);
    }

    void setCurve(float curve)
    {
        mCurve = curve;
        mEnv.curve(mCurve);
    }

    void trigger()
    {
        mEnv.reset();
    }

    void reset()
    {
        mEnv.reset();
    }

    bool done() const
    {
        return mEnv.done();
    }

    float process() override
    {
        return mEnv();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

    
private:
    gam::AD<double, double> mEnv;

    float mAttack;
    float mDecay;
    float mLevel;
    float mCurve;
};

class ADSR : public Generator {
public:
    ADSR(float attack  = 0.01f,
         float decay   = 0.1f,
         float sustain = 0.7f,
         float release = 0.3f,
         float level   = 1.0f,
         float curve   = -4.0f)
    : mAttack(attack),
    mDecay(decay),
    mSustain(sustain),
    mRelease(release),
    mLevel(level),
    mCurve(curve)
    {
        mEnv.attack(mAttack);
        mEnv.decay(mDecay);
        mEnv.sustain(mSustain);
        mEnv.release(mRelease);
        mEnv.amp(mLevel);
        mEnv.curve(mCurve);

        mEnv.reset();
    }

    void setAttack(float seconds)
    {
        mAttack = std::max(0.f, seconds);
        mEnv.attack(mAttack);
    }

    void setDecay(float seconds)
    {
        mDecay = std::max(0.f, seconds);
        mEnv.decay(mDecay);
    }

    void setSustain(float level)
    {
        mSustain = std::clamp(level, 0.f, 1.f);
        mEnv.sustain(mSustain);
    }

    void setRelease(float seconds)
    {
        mRelease = std::max(0.f, seconds);
        mEnv.release(mRelease);
    }

    void setLevel(float level)
    {
        mLevel = std::max(0.f, level);
        mEnv.amp(mLevel);
    }

    void setCurve(float curve)
    {
        mCurve = curve;
        mEnv.curve(mCurve);
    }

    void noteOn()
    {
        mEnv.reset();
    }

    void noteOff()
    {
        mEnv.release();
    }

    void reset()
    {
        mEnv.reset();
    }

    bool done() const
    {
        return mEnv.done();
    }

    float process() override
    {
        return mEnv();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }


private:
    gam::ADSR<double, double> mEnv;

    float mAttack;
    float mDecay;
    float mSustain;
    float mRelease;
    float mLevel;
    float mCurve;
};

class AllPass1Block : public Function {
public:
    AllPass1Block(float initFreq = 1000.0f, int N = 4)
    : mBaseFreq(initFreq), mDetune(0.0f)
    {
        const int stages = std::max(1, N);

        // Phase-90 style geometric spread
        const float baseSpread = 0.07f;  // ±7%
        std::mt19937 rng(1337);
        std::uniform_real_distribution<float> tol(0.99f, 1.01f);

        mRatios.resize(stages);

        for (int i = 0; i < stages; ++i)
        {
            float pos = (float(i) / (stages - 1)) - 0.5f;
            float spread = std::pow(1.0f + baseSpread, pos * 2.0f);
            float jitter = tol(rng);

            mRatios[i] = spread * jitter;
            mFilters.emplace_back(initFreq);
        }

        freq(initFreq);
    }

    // ---------------- Controls ----------------

    void freq(float freqHz)
    {
        mBaseFreq = std::max(1.f, freqHz);
        updateStages();
    }

    float freq() const { return mBaseFreq; }

    void setDetune(float d)   // 0…1
    {
        mDetune = std::clamp(d, 0.f, 1.f);
        updateStages();
    }

    float detune() const { return mDetune; }

    void reset()
    {
        for (auto& f : mFilters)
            f.reset();
    }

    // ---------------- DSP ----------------

    float process(float input) override
    {
        float r = mFilters[0](input);
        for (size_t i = 1; i < mFilters.size(); ++i)
            r = mFilters[i](r);
        return r;
    }

    void run(const float* input, float* output, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    void updateStages()
    {
        float nyq = 0.49f * gam::sampleRate();

        for (size_t i = 0; i < mFilters.size(); ++i)
        {
            float ratio = 1.0f + mDetune * (mRatios[i] - 1.0f);
            float f = std::clamp(mBaseFreq * ratio, 1.f, nyq);
            mFilters[i].freq(f);
        }
    }

private:
    std::vector<gam::AllPass1<double>> mFilters;
    std::vector<float> mRatios;

    float mBaseFreq = 1000.0f;
    float mDetune   = 0.0f;
};

class AllPass2Block : public Function {
public:
    AllPass2Block(float initFreq = 1000.0f, float initWidth = 100.0f, int N = 4)
    : mBaseFreq(initFreq), mBaseWidth(initWidth), mDetune(0.0f)
    {
        const int stages = std::max(1, N);

        // Phase-90 style spread
        const float baseSpread = 0.07f; // ±7%
        std::mt19937 rng(1337);
        std::uniform_real_distribution<float> tolF(0.99f, 1.01f);
        std::uniform_real_distribution<float> tolW(0.98f, 1.02f);

        mRatios.resize(stages);
        mWidthRatios.resize(stages);

        for (int i = 0; i < stages; ++i)
        {
            float pos = (float(i) / (stages - 1)) - 0.5f;
            float spread = std::pow(1.0f + baseSpread, pos * 2.0f);

            mRatios[i]      = spread * tolF(rng);
            mWidthRatios[i] = tolW(rng);

            mFilters.emplace_back(initFreq, initWidth);
        }

        updateStages();
    }

    // ---------------- Controls ----------------

    void freq(float freqHz)
    {
        float nyq = 0.49f * gam::sampleRate();
        mBaseFreq = std::clamp(freqHz, 1.f, nyq);
        updateStages();
    }

    float freq() const { return mBaseFreq; }

    void width(float widthHz)
    {
        float nyq = 0.49f * gam::sampleRate();
        mBaseWidth = std::clamp(widthHz, 0.f, nyq);
        updateStages();
    }

    float width() const { return mBaseWidth; }

    void setDetune(float d) // 0…1
    {
        mDetune = std::clamp(d, 0.f, 1.f);
        updateStages();
    }

    float detune() const { return mDetune; }

    void reset()
    {
        for (auto& f : mFilters)
            f.reset();
    }

    // ---------------- DSP ----------------

    float process(float input) override
    {
        float r = mFilters[0](input);
        for (size_t i = 1; i < mFilters.size(); ++i)
            r = mFilters[i](r);
        return r;
    }

    void run(const float* input, float* output, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    void updateStages()
    {
        float nyq = 0.49f * gam::sampleRate();

        for (size_t i = 0; i < mFilters.size(); ++i)
        {
            float fr = 1.0f + mDetune * (mRatios[i] - 1.0f);
            float wr = 1.0f + mDetune * (mWidthRatios[i] - 1.0f);

            float f = std::clamp(mBaseFreq * fr, 1.f, nyq);
            float w = std::clamp(mBaseWidth * wr, 0.f, nyq);

            mFilters[i].freq(f);
            mFilters[i].width(w);
        }
    }

private:
    std::vector<gam::AllPass2<double>> mFilters;
    std::vector<float> mRatios;
    std::vector<float> mWidthRatios;

    float mBaseFreq  = 1000.0f;
    float mBaseWidth = 100.0f;
    float mDetune    = 0.0f;
};

class AllPass1 : public Function {
public:
    AllPass1(float initFreq = 1000.0f)
    : mFilter(initFreq)
    {
        
        freq(std::max(1.f, initFreq));    
    }

    void freq(float freqHz)
    {
        mFilter.freq(std::max(1.f, freqHz));
    }

    void reset()
    {
        mFilter.reset();
    }

    float high(float input)
    {
        return mFilter.high(input);
    }

    float low(float input)
    {
        return mFilter.low(input);
    }
        
    float process(float input) override
    {
        return mFilter(input);
    }

    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }

private:
    gam::AllPass1<double> mFilter;
};

class AllPass2 : public Function {
public:
    AllPass2(float initFreq = 1000.0f,
             float initWidth = 100.0f)
    : mFilter(initFreq, initWidth)
    {
        
    }

    void freq(float freqHz)
    {
        mFilter.freq(std::max(1.f, freqHz));    
    }

    void width(float widthHz)
    {
        mFilter.width(std::max(0.f, widthHz));    
    }

    void reset()
    {
        mFilter.reset();
    }

    float process(float input) override
    {    
        return mFilter(input);
    }


    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }

        
private:
    gam::AllPass2<double> mFilter;    
};


class AllPassFilter : public Function
{
public:
    AllPassFilter() {
        filter.type(gam::ALL_PASS);
    }

    void  freq(float f)  { filter.freq(f); }
    float freq() const   { return filter.freq(); }

    void  res(float r)   { filter.res(r); }
    float res() const    { return filter.res(); }

    void  level(float v){ filter.level(v); }
    float level() const  { return filter.level(); }

    float process(float input) override {
        return filter(input);
    }

    void reset() {
        filter.reset();
    }

private:
    gam::Biquad<double> filter;
};


class AM {
public:
    AM(float depth = 1.0f)
    : mAM(depth)
    {
    }

    void setDepth(float depth)
    {    
        mAM.depth(std::max(0.f, depth));
    }

    float process(float carrier, float modulator)
    {
        return mAM(carrier, modulator);
    }

    void run(const float* carrier,
                const float* modulator,
                float* output,
                size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(carrier[i], modulator[i]);
    }


    float mDepth = 1.0f;

private:
    gam::AM<double> mAM;
};

class BandPassFilter : public Function
{
public:
    BandPassFilter() {
        filter.type(gam::BAND_PASS);
    }

    void  freq(float f)  { filter.freq(f); }
    float freq() const   { return filter.freq(); }

    void  res(float r)   { filter.res(r); }
    float res() const    { return filter.res(); }

    void  level(float v){ filter.level(v); }
    float level() const  { return filter.level(); }

    float process(float input) override {
        return filter(input);
    }

    void reset() {
        filter.reset();
    }

private:
    gam::Biquad<double> filter;
};

class Biquad : public Function
{
public:
    using FilterType = gam::FilterType;

    Biquad(float initFreq = 1000.0f,
           float initRes  = 0.707f,           
           float initLevel = 1.0f,
           FilterType type = gam::LOW_PASS)
    {
        mFilter.set(initFreq, initRes, type);
        mFilter.level(initLevel);                
    }

    void setFreq(float f)    { mFilter.freq(f); }
    void setRes(float r)     { mFilter.res(r); }
    void setLevel(float l)   { mFilter.level(l); }
    void setType(FilterType t){ mFilter.type(t); }
    void setCoef(float a0, float a1, float a2, float b1, float b2)
    { mFilter.coef(a0, a1, a2, b1, b2); }

    void reset() { mFilter.reset(); }

    float getFreq()  const { return mFilter.freq(); }
    float getRes()   const { return mFilter.res(); }
    float getLevel() const { return mFilter.level(); }

    float process(float input) override
    {
        return mFilter(input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    float freq() { return mFilter.freq(); }
    void  freq(float f) { mFilter.freq(f); }
    float  res() { return mFilter.res(); }
    void  res(float r) { mFilter.res(r); }
private:
    gam::Biquad<double> mFilter;  
};

class Biquad3 : public Function {
public:
    using FilterType = gam::FilterType;

    /// Filter types (mirrors gam::FilterType)    
    Biquad3(float f0, float f1, float f2,
            float res = 8.0f,            
            float level = 1.0f,
            FilterType type = FilterType::BAND_PASS)
    : 
    mFilter(f0, f1, f2, res, type)
    {    
    
    }


    void setFreqs(float f0, float f1, float f2)
    {
        float mF0 = std::max(5.f, f0);
        float mF1 = std::max(5.f, f1);
        float mF2 = std::max(5.f, f2);

        mFilter.freq(mF0, mF1, mF2);
    }

    void setRes(float res)
    {
        float mRes = std::max(0.001f, res);
        mFilter.bq0.res(mRes);
        mFilter.bq1.res(mRes);
        mFilter.bq2.res(mRes);
    }

    void setLevel(float level)
    {
        float mLevel = std::max(0.f, level);
        mFilter.bq0.level(mLevel);
        mFilter.bq1.level(mLevel);
        mFilter.bq2.level(mLevel);
    }

    void setType(FilterType type, int filter)
    {    
        switch(filter)
        {
            case 0: mFilter.bq0.type(type); break;
            case 1: mFilter.bq1.type(type); break;
            case 2: mFilter.bq2.type(type); break;
            default:
                mFilter.bq0.type(type); break;
                mFilter.bq1.type(type); break;
                mFilter.bq2.type(type); break;        
        }
    }

    void reset()
    {
        mFilter.bq0.reset();
        mFilter.bq1.reset();
        mFilter.bq2.reset();
    }

    float process(float input) override
    {
        return mFilter(input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }


    void setType0(FilterType type) { mFilter.bq0.type(type); }
    void setType1(FilterType type) { mFilter.bq1.type(type); }
    void setType2(FilterType type) { mFilter.bq2.type(type); }

private:
    gam::Biquad3 mFilter;  
};

class BlockDC : public Function {
public:
    BlockDC(float width = 35.0f)
    : mFilter(width)
    {
        
    }

    void setWidth(float width)
    {
        mFilter.width(width);
    }

    void reset()
    {
        mFilter.reset();
    }

    float process(float input) override
    {
        return mFilter(input);
    }

    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }
        
private:
    gam::BlockDC<double> mFilter;
};

class BlockNyq : public Function {
public:
    BlockNyq(float width = 35.0f)
    : mFilter(width)
    {
        
    }

    void setWidth(float width)
    {
        mFilter.width(width);
    }

    void reset()
    {
        mFilter.reset();
    }

    float process(float input) override
    {
        return mFilter(input);
    }

    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }

private:
    gam::BlockNyq<double> mFilter;    
};

class Burst : public Generator {
public:
    Burst(float startFreq = 20000.f,
          float endFreq   = 4000.f,
          float decay     = 0.1f,
          float resonance = 2.0f)
    : mBurst(startFreq, endFreq, decay, resonance)
    {
        
    }

    void trigger(float startFreq,
                        float endFreq,
                        float decay,
                        bool resetState=true)
    {
        mBurst(startFreq, endFreq, decay, resetState);
    }

    void reset()
    {
        mBurst.reset();
    }

    float process() override
    {
        return mBurst();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::Burst mBurst;
  
};

class Buzz : public Generator {
public:
    Buzz(float freq = 440.f,
         float phase = 0.f,
        float harmonics = 8.f)
    : mOsc(freq, phase, harmonics)
    {
        
    }

    void setFreq(float f)
    {
        mOsc.freq(f);
    }

    void setPeriod(float seconds)
    {
        if(seconds > 0.f)
            mOsc.freq(1.f / seconds);
    }

    void setHarmonics(float h)
    {
        mOsc.harmonics(h);
    }

    void setHarmonicsMax()
    {
        mOsc.harmonicsMax();
    }

    void antialias()
    {
        mOsc.antialias();
    }

    void normalize(bool enable)
    {
        mOsc.normalize(enable);
    }

    float process() override
    {
        return mOsc();
    }

    float saw()
    {
        return mOsc.saw(integrate);
    }

    float square()
    {
        return mOsc.square(integrate);
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::Buzz<double> mOsc;
    float integrate = 0.999f;
};

class Celeste : public Function {
public:
    Celeste(float delay  = 0.04f,
            float amount = 0.004f,
            float freq   = 0.6f)
    {    
        mCeleste.delay(delay).amount(amount).freq(freq);
    }

    void setDelay(float seconds)
    {
        float mDelay = std::max(0.f, seconds);
        mCeleste.delay(mDelay);
    }

    void setAmount(float seconds)
    {
        float mAmount = std::max(0.f, seconds);
        mCeleste.amount(mAmount);
    }

    void setFreq(float hz)
    {
        float mFreq = std::max(0.f, hz);
        mCeleste.freq(mFreq);
    }

    float process(float input) override
    {
        return mCeleste(input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    gam::Celeste<double> mCeleste;    
};

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

class Chorus : public Function {
public:
    Chorus(float delay = 0.0021f,
           float depth = 0.002f,
           float freq  = 1.0f,
           float ffd   = 0.9f,
           float fbk   = 0.1f)
    :   
    mChorus(delay, depth, freq, ffd, fbk)
    {
    
    }

    void setDelay(float seconds)
    {
        float mDelay = std::max(0.f, seconds);
        mChorus.delay(mDelay);
    }

    void setDepth(float seconds)
    {
        float mDepth = std::max(0.f, seconds);
        mChorus.depth(mDepth);
    }

    void setFreq(float hz)
    {
        float mFreq = std::max(0.f, hz);
        mChorus.freq(mFreq);
    }

    void setFeedforward(float v)
    {
        float mFFD = std::clamp(v, -1.f, 1.f);
        mChorus.ffd(mFFD);
    }

    void setFeedback(float v)
    {
        float mFBK = std::clamp(v, -0.999f, 0.999f);
        mChorus.fbk(mFBK);
    }

    void setMaxDelay(float seconds)
    {
        mChorus.maxDelay(std::max(0.f, seconds));
    }

    float process(float input) override
    {
        return mChorus(input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    gam::Chorus<double> mChorus;   
};

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

class CombFilter : public Function
{
public:
    enum Mode { FEEDFORWARD, FEEDBACK, ALLPASS, PHYSICAL };

    CombFilter(float maxDelaySec = 2.0f,
               float initFreqHz = 440.0f,
               float initRes01  = 0.25f,
               Mode  mode = FEEDBACK)
    : mComb(maxDelaySec, 1.0f / std::max(20.0f, initFreqHz))
    {
        mMaxDelay = std::max(0.001f, maxDelaySec);
        baseCutoff = initFreqHz;
        baseRes    = std::clamp(initRes01, 0.0f, 1.0f);

        setMode(mode);
        setAM(0.0f);
    }

    // ----- Filter-like API -----
    void setFreq(float hz) { baseCutoff = hz; }
    void setRes (float r)  { baseRes = std::clamp(r, 0.f, 1.f); }
    void setWet (float w)  { mWet = std::clamp(w, 0.f, 1.f); }
    void setFM(float v)    { cutoff.set(v);}
    void setQM(float v)    { res.set(v); }
    void setAM(float v)    { am.set(v);}

    void setMode(Mode m)
    {
        mMode = m;
        mFbMax = mFfMax = mApMax = 0.0f;

        if (m == FEEDBACK || m == PHYSICAL) mFbMax = 0.995f;
        if (m == FEEDFORWARD)               mFfMax = 0.95f;
        if (m == ALLPASS)                   mApMax = 0.95f;
    }

    void setDamp(float d) { mDampTarget = std::clamp(d, 0.f, 1.f); }
    void setInvertFeedback(bool v) { mInvertFb = v; }

    float process(float x) override
    {
        // ---- modulation ----
        float fMod = cutoff.process();
        float rMod = res.process();
        float aMod = am.process();

        float freq = baseCutoff * (1.0f + fMod);
        float reso = baseRes    * (1.0f + rMod);

        freq = std::clamp(freq, 20.0f, 1.0f / mMinDelay);
        reso = std::clamp(reso, 0.0f, 1.0f);

        updateTargets(freq, reso);

        // ---- apply params ----
        mComb.setDelay(mDelay);
        mComb.setFeedback(mFb);
        mComb.setFeedforward(mFf);
        mComb.setAllPass(mAp);

        float y = 0.0f;

        if (mMode == PHYSICAL)
        {
            float d = mComb.read();
            float fb = mInvertFb ? -mFb : mFb;

            float a = 0.001f + (1.0f - mDampTarget) * 0.2f;
            mDampZ += a * (d - mDampZ);

            mComb.write(x + fb * mDampZ);
            y = d;
        }
        else
        {
            y = mComb.process(x);
        }

        float out = (1.0f - mWet) * x + mWet * y;
        return out * aMod;
    }

    Modulator cutoff, res, am;

private:
    void updateTargets(float freq, float res01)
    {
        float delay = 1.0f / std::max(freq, 1e-6f);
        delay = std::clamp(delay, mMinDelay, mMaxDelay);

        constexpr float k = 6.0f;
        float shaped = 1.0f - std::exp(-k * res01);

        mFb = shaped * mFbMax;
        mFf = shaped * mFfMax;
        mAp = shaped * mApMax;
        ///mDamp = mDampTarget;

        mFb = std::clamp(mFb, -0.999f, 0.999f);
        mFf = std::clamp(mFf, -0.999f, 0.999f);
        mAp = std::clamp(mAp, -0.999f, 0.999f);
        mDelay = delay;
    }

private:
    Comb  mComb;    
    Mode  mMode = FEEDBACK;

    float baseCutoff;
    float baseRes;

    float mWet = 1.0f;
    bool  mInvertFb = false;

    float mMaxDelay = 2.0f;
    float mMinDelay = 1.0f / 8000.0f;

    float mFbMax = 0.0f;
    float mFfMax = 0.0f;
    float mApMax = 0.0f;

    float mDelay = 0.01f;
    float mFb = 0.0f;
    float mFf = 0.0f;
    float mAp = 0.0f;
    float mDampTarget = 0.5f;
    float mDampZ = 0.0f;
};

class CSine : public Generator {
public:
    CSine(float _freq = 440.f,
          float _amp  = 1.f,
          float _decay60 = -1.f,
          float _phase = 0.f)
    : mOsc(_freq, _amp, _decay60, _phase)
    {
        
    }


    void set(float f, float p, float a, float d=-1.f)
    {    
        mOsc.freq(f);    
        mOsc.amp(a);
        mOsc.decay(d);
        mOsc.phase(p);
    }

    void setFreq(float f)    { mOsc.freq(f); }
    void setPhase(float p)   { mOsc.phase(p); }
    void setAmp(float a)     { mOsc.amp(a);}
    void setDecay(float d)   { mOsc.decay(d);}
    float getFreq() const    { return mOsc.freq(); }
    float getAmp() const     { return mOsc.amp(); }
    float getDecay() const   { return mOsc.decay(); }

    float process() override
    {
        auto c = mOsc();
        return static_cast<float>(c.r);
    }

    ComplexSample processComplex() {
        auto r = mOsc();
        return ComplexSample(r.r,r.i);
    }
    
    float processImag()
    {
        auto c = mOsc.val;
        return static_cast<float>(c.i);
    }

    void reset()
    {
        mOsc.reset();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

    
private:
    gam::CSine<double> mOsc;
};

class Curve : public Generator {
public:
    Curve(float lengthSeconds = 0.1f,
          float curvature = -4.0f,
          float start = 0.0f,
          float end   = 1.0f)
    : mLength(lengthSeconds),
    mCurvature(curvature),
    mStart(start),
    mEnd(end)
    {    
        mCurve.set(mLength * gam::sampleRate(), mCurvature, mStart, mEnd);
    }

    void set(float lengthSeconds, float curvature, float start, float end)
    {
        mLength    = std::max(0.f, lengthSeconds);
        mCurvature = curvature;
        mStart     = start;
        mEnd       = end;

        mCurve.set(mLength * gam::sampleRate(), mCurvature, mStart, mEnd);
    }

    void setLength(float lengthSeconds)
    {
        mLength = std::max(0.f, lengthSeconds);
        mCurve.set(mLength * gam::sampleRate(), mCurvature, mStart, mEnd);
    }

    void setCurvature(float curvature)
    {
        mCurvature = curvature;
        mCurve.set(mLength * gam::sampleRate(), mCurvature, mStart, mEnd);
    }

    void setLevels(float start, float end)
    {
        mStart = start;
        mEnd   = end;
        mCurve.levels(mStart, mEnd);
    }

    void reset()
    {
        mCurve.reset();
    }

    bool done() const
    {
        return mCurve.done();
    }

    float value() const
    {
        return mCurve.value();
    }

    float process() override
    {
        return mCurve();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

    ~Curve() {}
    
    float mLength;
    float mCurvature;
    float mStart;
    float mEnd;

private:
    gam::Curve<double, double> mCurve;    
};

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
    gam::Delay<double> mDelay;    
};

class Differencer : public Function {
public:
    Differencer()
    : mFilter()
    {
        
    }

    float process(float input) override
    {
        return mFilter(input);
    }

    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }


private:
    gam::Differencer<double> mFilter;
};

class DSF : public Generator {
public:
    DSF(float freq = 440.f,
        float freqRatio = 1.f,
        float ampRatio = 0.5f,
        float harmonics = 8.f)
    : mOsc(freq, freqRatio, ampRatio, harmonics)
    {
        
    }

    void setFreq(float f)
    {
        mOsc.freq(f);
    }

    void setPeriod(float seconds)
    {
        if(seconds > 0.f)
            mOsc.freq(1.f / seconds);
    }

    void setFreqRatio(float r)
    {
        mOsc.freqRatio(r);
    }

    void setAmpRatio(float r)
    {
        mOsc.ampRatio(r);
    }

    void setHarmonics(float h)
    {
        mOsc.harmonics(h);
    }

    void setHarmonicsMax()
    {
        mOsc.harmonicsMax();
    }

    void antialias()
    {
        mOsc.antialias();
    }

    float process() override
    {
        return mOsc();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::DSF<double> mOsc;
};

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

class EnvFollow : public Function {
public:
    
    EnvFollow(float freq=10.0f)
    : 
    mFreq(freq),
    mEnv(freq)
    {
    }

    void setFreq(float freqHz)
    {
        mFreq = std::max(0.01f, freqHz);
        mEnv.lpf.freq(mFreq);
    }

    void setLag(float seconds)
    {
        float f = (seconds > 0.0f) ? (1.0f / seconds) : 1000.0f;
        setFreq(f);
    }

    void reset()
    {
        mEnv.lpf.reset(0.0f);
    }

    float process(float input) override
    {
        return mEnv(input);
    }

    float value() const
    {
        return mEnv.value();
    }

    bool done(float eps=0.001f) const
    {
        return mEnv.done(eps);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    float mFreq = 10.0f;
    float mLag  = 0.0;

private:
    gam::EnvFollow<double> mEnv;
    
};

class FreqShift : public Function {
public:
    FreqShift(float shiftHz = 0.0f)
    : 
    mShift(shiftHz)
    {
        
    }

    void setShift(float shiftHz)
    {    
        mShift.freq(shiftHz);
    }

    float process(float input) override
    {
        return mShift(input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:

    gam::FreqShift<double> mShift;
};


// ============================================================
// Utility DSP Modules v0A
// Simple SWIG/introspection-friendly math processors.
// These are intentionally small Function/Function2 wrappers so
// PatchEngine/DSL/JSON patches can compose gain, offset, addition,
// and multiplication without Python-side special cases.
// ============================================================

class Gain : public Function {
public:
    Gain(float initGain = 1.0f)
    : mGain(initGain)
    {
    }

    void setGain(float v) { mGain = v; }
    void gain(float v) { setGain(v); }
    float gain() const { return mGain; }

    float process(float input) override
    {
        return input * mGain;
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    float mGain = 1.0f;
};

class Offset : public Function {
public:
    Offset(float initOffset = 0.0f)
    : mOffset(initOffset)
    {
    }

    void setOffset(float v) { mOffset = v; }
    void offset(float v) { setOffset(v); }
    float offset() const { return mOffset; }

    float process(float input) override
    {
        return input + mOffset;
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    float mOffset = 0.0f;
};

class AddF : public Function2 {
public:
    AddF(float initValue = 0.0f)
    : mValue(initValue)
    {
    }

    void setValue(float v) { mValue = v; }
    void value(float v) { setValue(v); }
    float value() const { return mValue; }

    // Function path: add stored value.
    float process(float input) override
    {
        return input + mValue;
    }

    // Function2 path: add explicit second input.
    float process(float input, float value) override
    {
        return input + value;
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    float mValue = 0.0f;
};

class MultiplyF : public Function2 {
public:
    MultiplyF(float initFactor = 1.0f)
    : mFactor(initFactor)
    {
    }

    void setFactor(float v) { mFactor = v; }
    void factor(float v) { setFactor(v); }
    float factor() const { return mFactor; }

    // Function path: multiply by stored factor.
    float process(float input) override
    {
        return input * mFactor;
    }

    // Function2 path: multiply by explicit second input.
    float process(float input, float factor) override
    {
        return input * factor;
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    float mFactor = 1.0f;
};


// ============================================================
// Utility DSP Modules v0B
// Dry/wet, constant generation, generator summing, and two-input
// style mixing helpers. These are SWIG/introspection-friendly and
// keep the same constructor-args + key=value setter pattern used by
// PatchEngine, PatchDSL, and patch_json.
// ============================================================

class DryWet : public Function2 {
public:
    DryWet(float initMix = 0.5f, float initWet = 0.0f)
    : mMix(std::clamp(initMix, 0.0f, 1.0f)),
      mWet(initWet)
    {
    }

    void setMix(float v) { mMix = std::clamp(v, 0.0f, 1.0f); }
    void mix(float v) { setMix(v); }
    float mix() const { return mMix; }

    void setWet(float v) { mWet = v; }
    void wet(float v) { setWet(v); }
    float wet() const { return mWet; }

    float process(float input) override
    {
        return process(input, mWet);
    }

    float process(float dry, float wetSignal) override
    {
        return dry * (1.0f - mMix) + wetSignal * mMix;
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    float mMix = 0.5f;
    float mWet = 0.0f;
};

class ConstantGenerator : public Generator {
public:
    ConstantGenerator(float initValue = 0.0f)
    : mValue(initValue)
    {
    }

    void setValue(float v) { mValue = v; }
    void value(float v) { setValue(v); }
    float value() const { return mValue; }

    float process() override
    {
        return mValue;
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    float mValue = 0.0f;
};

class GeneratorBlock : public Generator {
public:
    GeneratorBlock() = default;

    void addGenerator(Generator* gen)
    {
        if(gen != nullptr)
            nodes.push_back(gen);
    }

    void clear()
    {
        nodes.clear();
    }

    size_t size() const
    {
        return nodes.size();
    }

    void setNormalize(bool v)
    {
        mNormalize = v;
    }

    void normalize(bool v)
    {
        setNormalize(v);
    }

    bool normalize() const
    {
        return mNormalize;
    }

    float process() override
    {
        if(nodes.empty())
            return 0.0f;

        float sum = 0.0f;
        for(auto* gen : nodes)
            sum += gen->process();

        if(mNormalize)
            sum *= 1.0f / std::sqrt(std::max<size_t>(1, nodes.size()));

        return sum;
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    std::vector<Generator*> nodes;
    bool mNormalize = true;
};

class Mix2 : public Function2 {
public:
    Mix2(float initGainA = 1.0f,
         float initGainB = 1.0f,
         float initB = 0.0f,
         float initBias = 0.0f)
    : mGainA(initGainA),
      mGainB(initGainB),
      mB(initB),
      mBias(initBias)
    {
    }

    void setGainA(float v) { mGainA = v; }
    void gainA(float v) { setGainA(v); }
    float gainA() const { return mGainA; }

    void setGainB(float v) { mGainB = v; }
    void gainB(float v) { setGainB(v); }
    float gainB() const { return mGainB; }

    void setB(float v) { mB = v; }
    void b(float v) { setB(v); }
    float b() const { return mB; }

    void setBias(float v) { mBias = v; }
    void bias(float v) { setBias(v); }
    float bias() const { return mBias; }

    float process(float input) override
    {
        return process(input, mB);
    }

    float process(float a, float bValue) override
    {
        return a * mGainA + bValue * mGainB + mBias;
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    float mGainA = 1.0f;
    float mGainB = 1.0f;
    float mB = 0.0f;
    float mBias = 0.0f;
};

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
        return gam::scl::dBToAmp<float>(in);
    }
};

class AmpToDbF : public Function {
public:
    float process(float in) override {
        return gam::scl::ampTodB<float>(in);
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

class HighPassFilter: public Function
{
public:

    HighPassFilter() {
        filter.type(gam::FilterType::HIGH_PASS);
    }
    void freq(float f) { filter.freq(f); }
    float freq()       { return filter.freq(); }
    void res(float r)  { filter.res(r); }
    float res()        { return filter.res(); }
    void level(float v) { filter.level(v); }
    float level() { return filter.level(); }

    float process(float input) override { return filter(input); }

protected:
    gam::Biquad<double> filter;
};

class HighShelfFilter : public Function
{
public:
    HighShelfFilter() {
        filter.type(gam::HIGH_SHELF);
    }

    void  freq(float f)  { filter.freq(f); }
    float freq() const   { return filter.freq(); }

    void  res(float r)   { filter.res(r); }
    float res() const    { return filter.res(); }

    void  level(float v){ filter.level(v); }
    float level() const  { return filter.level(); }

    float process(float input) override {
        return filter(input);
    }

    void reset() {
        filter.reset();
    }

private:
    gam::Biquad<double> filter;
};

class Hilbert {
public:
    Hilbert()
    {
        
    }

    void reset()
    {
        mFilter.reset();
    }

    ComplexSample process(float input) {
        gam::Complex<double> v = mFilter(input);
        return ComplexSample(v.r,v.i);
    }
    void run(const float* input,
                    float* out_real,
                    float* out_imag,
                    size_t n)
    {
        for(size_t i = 0; i < n; ++i)
        {
            gam::Complex<double> v = mFilter(input[i]);
            out_real[i] = v.r;
            out_imag[i] = v.i;
        }
    }

private:
    gam::Hilbert<double> mFilter;
};

class Impulse : public Generator {
public:
    Impulse(float freq = 440.f,
            float phase = 0.f)
    : mOsc(freq, phase)
    {
        
    }

    void setFreq(float f)
    {
        mOsc.freq(f);
    }

    void setPeriod(float seconds)
    {
        if(seconds > 0.f)
            mOsc.freq(1.f / seconds);
    }

    float process() override
    {
        return mOsc();
    }

    float saw()
    {
        return mOsc.saw(integrate);
    }

    float square()
    {
        return mOsc.square(integrate);
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::Impulse<double> mOsc;
    float integrate = 0.999f;
};

class Inspector: public Function {
public:
    
    Inspector()
    : mInspector()
    {
    }

    float process(float input) override
    {
        return (float)mInspector(input);
    }

    void run(const float* input, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            process(input[i]);
    }

    void reset()
    {
        mInspector.reset();
    }

    void setDCThreshold(float v)
    {
        mInspector.DCThresh(v);
    }

    void setName(const char* name)
    {
        mInspector.name(name);
    }

    unsigned status() const
    {
        return mInspector.status();
    }

    bool hasError() const
    {
        return status() != 0;
    }

    float peak() const
    {
        return mInspector.peak();
    }

    float dc() const
    {
        return mInspector.DC();
    }

    float maxDC() const
    {
        return mInspector.maxDC();
    }

    const char* name() const
    {
        return mInspector.name();
    }

    void Print(bool showName) const
    {
        mInspector.print(showName);
    }

private:
    gam::Inspector mInspector;
};

class Integrator : public Function {
public:
    Integrator(float leak = 1.0f,
               float init = 0.0f)
    : mFilter(leak, init)
    {
        
    }

    void setLeak(float leak)
    {
        mFilter.leak(leak);
    }

    void reset()
    {
        mFilter.reset();
    }

    float process(float input) override
    {
        return mFilter(input);
    }

    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }
    
private:
    gam::Integrator<double> mFilter;
    
};

class LFO : public Generator {
public:
    enum Wave {
        SINE,
        SINE_PARA,
        COS,
        COS_CUBIC,
        SAW_DOWN,
        SAW_UP,
        TRIANGLE,
        SQUARE,
        PULSE,
        PARABOLA,
        STAIR,
        IMPULSE,
        HANN,
        EVEN3,
        EVEN5,
        LINE2,
        UP2,
        PATTERN,
        S1,
        C2,
        S3,
        C4,
        S5,
        COS_CUBU,
        DOWNU,
        IMPU,
        LINE2U,
        PARAU,
        STAIRU,
        TRIU,
        UPU,
        UP2U
    };

    LFO(float freq = 1.f,
        float phase = 0.f,
        float mod = 0.5f,
        Wave wave=SINE)
    : mLfo(freq, phase, mod), mWave(wave)
    {
        
    }

    void setFreq(float f)
    {
        mLfo.freq(f);
    }

    void setPeriod(float seconds)
    {
        if(seconds > 0.f)
            mLfo.freq(1.f / seconds);
    }

    void setPhase(float p)
    {
        mLfo.phase(p);
    }

    void setMod(float m)
    {
        mLfo.mod(m);
    }

    void setWave(Wave w)
    {
        mWave = w;
    }

    float tick()
    {
        switch(mWave)
        {
            case SINE:        return mLfo.sin();
            case SINE_PARA:   return mLfo.sinPara();
            case COS:         return mLfo.cos();
            case COS_CUBIC:   return mLfo.cosCub();
            case SAW_DOWN:    return mLfo.down();
            case SAW_UP:      return mLfo.up();
            case TRIANGLE:    return mLfo.tri();
            case SQUARE:      return mLfo.sqr();
            case PULSE:       return mLfo.pulse();
            case PARABOLA:    return mLfo.para();
            case STAIR:       return mLfo.stair();
            case IMPULSE:     return mLfo.imp();
            case HANN:        return mLfo.hann();
            case EVEN3:       return mLfo.even3();
            case EVEN5:       return mLfo.even5();
            case LINE2:       return mLfo.line2();
            case UP2:         return mLfo.up2();
            case PATTERN:     return mLfo.patU() * 2.f - 1.f;
            case S1:          return mLfo.S1();
            case C2:          return mLfo.C2();
            case S3:          return mLfo.S3();
            case C4:          return mLfo.C4();
            case S5:          return mLfo.S5();
            case COS_CUBU:    return mLfo.cosCubU();
            case DOWNU:       return mLfo.downU();
            case IMPU:        return mLfo.impU();
            case LINE2U:      return mLfo.line2U();
            case PARAU:       return mLfo.paraU();
            case STAIRU:      return mLfo.stairU();
            case TRIU:        return mLfo.triU();
            case UPU:         return mLfo.upU();
            case UP2U:        return mLfo.up2U();
            default:          return mLfo.sin();
        }
    }

    float process() override
    {
        float r = tick();
        if(bUnipolar) r = (1.0f + r)*0.5f;
        return r;
    }
    

    void reset()
    {
        mLfo.phase(0.f);
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

    void  setUnipolar(bool v) { bUnipolar = v; }
    void  phaseAdd(float p) { mLfo.phaseAdd(p); }
    void  freq(float p) { mLfo.freq(p);}
    float freq() { return mLfo.freq(); }

    float operator()() { return process(); }

private:
    gam::LFO<> mLfo;
    Wave mWave;
    bool bUnipolar = false;
};

class LowPassFilter : public Function
{
public:
    LowPassFilter() {
        filter.type(gam::LOW_PASS);
    }

    void freq(float f)  { filter.freq(f); }
    float freq() const  { return filter.freq(); }

    void res(float r)   { filter.res(r); }
    float res() const   { return filter.res(); }

    void level(float v) { filter.level(v); }
    float level() const { return filter.level(); }

    float process(float input) override {
        return filter(input);
    }

    void reset() {
        filter.reset();
    }

private:
    gam::Biquad<double> filter;
};

class LowShelfFilter : public Function
{
public:
    LowShelfFilter() {
        filter.type(gam::LOW_SHELF);
    }

    void  freq(float f)  { filter.freq(f); }
    float freq() const   { return filter.freq(); }

    void  res(float r)   { filter.res(r); }
    float res() const    { return filter.res(); }

    void  level(float v){ filter.level(v); }
    float level() const  { return filter.level(); }

    float process(float input) override {
        return filter(input);
    }

    void reset() {
        filter.reset();
    }

private:
    gam::Biquad<double> filter;
};

class MaxAbs: public Function {
public:
    MaxAbs(unsigned windowSize = 256)
    : mMax(windowSize)
    {
    }

    void reset()
    {
        mMax.reset();
    }

    void setWindow(unsigned windowSize)
    {
        mMax.period(windowSize);
    }

    float process(float input) override
    {
        return mMax(input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    float value() const
    {
        return mMax.value();
    }

    float valueN() const
    {
        return mMax.valueN();
    }

private:
    gam::MaxAbs<double> mMax;
};

class MonoSynth : public Generator {
public:
    MonoSynth(float freq  = 440.f,
              float dur   = 0.8f,
              float ctf1  = 1000.f,
              float ctf2  = 100.f,
              float res   = 3.0f)
    : 
    mSynth(freq, dur, ctf1, ctf2, res)
    {
        
    }

    void setFreq(float hz)
    {
        float mFreq = std::max(1.f, hz);
        mSynth.freq(mFreq);
    }

    void setCutoff(float fc)
    {
        mSynth.filter.freq(fc);    
    }

    void reset()
    {
        mSynth.reset();
    }

    float process() override
    {
        return mSynth();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::MonoSynth mSynth;    
};

class MovingAvg : public Function {
public:
    MovingAvg(unsigned size = 64)
    : mFilter(size)
    {
        
    }

    void resize(unsigned size)
    {
        mFilter.resize(size);
    }

    float process(float input)
    {
        return mFilter(input);
    }

    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }

private:
    gam::MovingAvg<double> mFilter;
};

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

//////////////////////////////////////////////////////////
// Base Noise Wrapper
//////////////////////////////////////////////////////////

class NoiseBase : public Generator {
public:
    virtual void seed(uint32_t v) = 0;
};

//////////////////////////////////////////////////////////
// White Noise
//////////////////////////////////////////////////////////

class NoiseWhite : public NoiseBase {
public:
    NoiseWhite(uint32_t seed = 0) { if(seed) mNoise.seed(seed); }

    void seed(uint32_t v) override { mNoise.seed(v); }

    float process() override {
        return mNoise();
    }

private:
    gam::NoiseWhite<> mNoise;
};

//////////////////////////////////////////////////////////
// Pink Noise
//////////////////////////////////////////////////////////

class NoisePink : public NoiseBase {
public:
    NoisePink(uint32_t seed = 0) : mNoise(seed) {}

    void seed(uint32_t v) override { mNoise.seed(v); }

    float process() override {
        return mNoise();
    }

private:
    gam::NoisePink<> mNoise;
};

//////////////////////////////////////////////////////////
// Brown Noise
//////////////////////////////////////////////////////////

class NoiseBrown : public NoiseBase {
public:
    NoiseBrown(float start = 0.0f,
               float step = 0.04f,
               float min = -1.0f,
               float max = 1.0f,
               uint32_t seed = 0)
    : mNoise(start, step, min, max, seed) {}

    void seed(uint32_t v) override { mNoise.seed(v); }

    void setStep(float v) { mNoise.step = v; }
    void setRange(float mn, float mx) { mNoise.min = mn; mNoise.max = mx; }

    float process() override {
        return mNoise();
    }

private:
    gam::NoiseBrown<> mNoise;
};

//////////////////////////////////////////////////////////
// Violet Noise
//////////////////////////////////////////////////////////

class NoiseViolet : public NoiseBase {
public:
    NoiseViolet(uint32_t seed = 0) { if(seed) mNoise.seed(seed); }

    void seed(uint32_t v) override { mNoise.seed(v); }

    float process() override {
        return mNoise();
    }

private:
    gam::NoiseViolet<> mNoise;
};

//////////////////////////////////////////////////////////
// Binary Noise
//////////////////////////////////////////////////////////

class NoiseBinary : public NoiseBase {
public:
    NoiseBinary(float amp = 1.0f, uint32_t seed = 0)
    : mNoise(amp, seed) {}

    void seed(uint32_t v) override { mNoise.seed(v); }

    void setAmp(float v) { mNoise.amp = v; }

    float process() override {
        return mNoise();
    }

private:
    gam::NoiseBinary<> mNoise;
};


class NotchFilter : public Function
{
public:
    NotchFilter() {
        filter.type(gam::BAND_REJECT);
    }

    void  freq(float f)  { filter.freq(f); }
    float freq() const   { return filter.freq(); }

    void  res(float r)   { filter.res(r); }
    float res() const    { return filter.res(); }

    void  level(float v){ filter.level(v); }
    float level() const  { return filter.level(); }

    float process(float input) override {
        return filter(input);
    }

    void reset() {
        filter.reset();
    }

private:
    gam::Biquad<double> filter;
};

class Notch : public Function {
public:
    Notch(float initFreq = 1000.0f,
          float initWidth = 100.0f)
    : mFilter(initFreq, initWidth)
    {
        
    }

    void reset()
    {
        mFilter.reset();
    }

    void setFreq(float freqHz)
    {
        mFilter.freq(std::clamp(freqHz, 0.f, (float)gam::sampleRate()*0.5f));    
    }

    void setWidth(float widthHz)
    {
        mFilter.width(std::clamp(widthHz, 0.f, (float)gam::sampleRate()*0.5f));    
    }

    float process(float input) override
    {
        return mFilter(input);
    }

    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }

private:
    gam::Notch<double> mFilter;
    
};

class OnePole : public Function {
public:
    
    OnePole(float initFreq = 1000.0f,
            gam::FilterType type = gam::LOW_PASS,
            float stored = 0.0f)
    {
        setFreq(initFreq);
        setType(type);
    }

    void setFreq(float freqHz)
    {
        mFilter.freq(freqHz);
    }

    void setType(gam::FilterType type)
    {    
        mFilter.type(type);
    }

    void lag(float lengthSeconds, float threshold=0.001f)
    {
        mFilter.lag(lengthSeconds, threshold);
    }

    void reset(float value=0.0f)
    {
        mFilter.reset(value);
    }

    float last() const
    {
        return mFilter.last();
    }

    float process(float input) override
    {    
        return mFilter(input);
    }

    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }

    float getFreq() { return mFilter.freq(); }

private:
    gam::OnePole<double> mFilter;    
};

class Osc : public Generator {
public:
    Osc(float freq = 440.f,
        float phase = 0.f,
        unsigned tableSize = 512)
    : mOsc(freq, phase, tableSize)
    {
    
    }

    void setFreq(float f) { mOsc.freq(f); } 
    void setPeriod(float seconds) {
        if(seconds > 0.f)
            mOsc.freq(1.f / seconds);
    }   
    void setPhase(float p) { mOsc.phase(p); }
    float freq() const { return mOsc.freq(); }
    float phase() const { return mOsc.phase(); }
    void clearTable()
    {
        for(unsigned i = 0; i < mOsc.size(); ++i)
            mOsc[i] = 0.f;
    }
    void addSine(float cycles, float amplitude, float phase)
    {
        mOsc.addSine(cycles, amplitude, phase);
    }
    float getTableSample(unsigned index) const
    {
        if(index < mOsc.size())
            return mOsc[index];
        return 0.f;
    }

    void setTableSample(unsigned index, float value)
    {
        if(index < mOsc.size())
            mOsc[index] = value;
    }

    unsigned tableSize() const { return mOsc.size(); }

    float tick() { return mOsc(); }
    void reset() { mOsc.reset(); }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }
    
    float process() override
    {
        return mOsc();
    }

    void phaseAdd(float p) { mOsc.phaseAdd(p); }
    void freq(float f) { mOsc.freq(f); }

private:
    gam::Osc<double> mOsc;
};


class Pan {
public:
    Pan(float pos = 0.0f)
    :   
    mPan(pos)
    {
    }

    void setPos(float pos)
    {
        float mPos = std::clamp(pos, -1.f, 1.f);
        mPan.pos(mPos);
    }

    void setPosGain(float pos, float gain)
    {
        float mPos = std::clamp(pos, -1.f, 1.f);
        mPan.pos(mPos, std::max(0.f, gain));
    }

    void setPosLinear(float pos)
    {
        float mPos = std::clamp(pos, -1.f, 1.f);
        mPan.posL(mPos);
    }

    StereoSample process(float input)
    {
        StereoSample s;
        mPan(input, s.L, s.R);
        return s;
    }
    
    void run(const float* input, float* left, float* right, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
        {
            StereoSample s= process(input[i]);
            left[i]  = s.L;
            right[i] = s.R;
        }
    }

private:
    gam::Pan<double> mPan;
};

class PCounter: public Generator {
public:
    PCounter(unsigned period = 256)
    : mCounter(period)
    {
    }

    void setPeriod(unsigned period)
    {
        mCounter.period(period);
    }

    unsigned period() const
    {
        return mCounter.period();
    }

    unsigned count() const
    {
        return mCounter.count();
    }

    bool cycled() const
    {
        return mCounter.cycled();
    }

    void reset()
    {
        mCounter.reset();
    }

    float process() override
    {
        return (float)mCounter();
    }

    void run(bool* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }


private:
    gam::PCounter mCounter;
};



class PeakingFilter : public Function
{
public:
    PeakingFilter() {
        filter.type(gam::PEAKING);
    }

    void  freq(float f)  { filter.freq(f); }
    float freq() const   { return filter.freq(); }

    void  res(float r)   { filter.res(r); }
    float res() const    { return filter.res(); }

    void  level(float v){ filter.level(v); }
    float level() const  { return filter.level(); }

    float process(float input) override {
        return filter(input);
    }

    void reset() {
        filter.reset();
    }

private:
    gam::Biquad<double> filter;
};

class Pluck : public Function {
public:
    Pluck(float freq  = 440.f,
          float decay = 0.99f)
    :   
    mPluck(freq, decay)
    {
    
    }

    void setFreq(float hz)
    {
        float mFreq = std::max(1.f, hz);
        mPluck.freq(mFreq);
    }

    void reset()
    {
        mPluck.reset();
    }

    float process() 
    {
        return mPluck();
    }

    float process(float input) override
    {
        return mPluck(input);
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }


private:
    gam::Pluck mPluck;    
};

class Quantizer : public Function {
public:
    Quantizer(float freq = 2000.f,
              float step = 0.f)
    : 
    mQuant(freq, step)
    {

    }

    void setFreq(float hz)
    {
        float mFreq = std::max(0.1f, hz);
        mQuant.freq(mFreq);
    }

    void setPeriod(float seconds)
    {
        mQuant.period(std::max(1e-6f, seconds));
    }

    void setStep(float step)
    {
        float mStep = std::max(0.f, step);
        mQuant.step(mStep);
    }

    float process(float input) override
    {
        return mQuant(input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    gam::Quantizer<double> mQuant;    
};

class Reson : public Function {
public:
    Reson(float initFreq = 1000.0f,
          float initWidth = 100.0f)
    : mFilter(initFreq, initWidth)
    {
        
    }

    void reset()
    {
        mFilter.reset();
    }

    void setFreq(float freqHz)
    {
        mFilter.freq(std::clamp(freqHz, 0.f, (float)gam::sampleRate()*0.5f));    
    }

    void setWidth(float widthHz)
    {
        mFilter.width(std::clamp(widthHz, 0.f, (float)gam::sampleRate()*0.5f));    
    }

    float process(float input)
    {
        return mFilter(input);
    }

    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }

private:
    gam::Reson<double> mFilter;
    
};

class Saw : public Generator {
public:
    Saw(float freq = 440.f,
        float phase = 0.f)
    : mOsc(freq, phase)
    {
        
    }

    void setFreq(float f)
    {
        mOsc.freq(f);
    }

    void setPeriod(float seconds)
    {
        if(seconds > 0.f)
            mOsc.freq(1.f / seconds);
    }

    float process() override
    {
        return mOsc(integrate);
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::Saw<double> mOsc;
    float integrate = 0.999f;
};

class Seg : public Generator {
public:
    Seg(float lengthSeconds = 0.1f,
        float start = 0.f,
        float end   = 1.f,
        float phase = 0.f)
    : mSeg(lengthSeconds, start, end, phase),
    mLength(lengthSeconds),
    mStart(start),
    mEnd(end)  
    {
        
    }

    void setLength(float seconds)
    {
        mLength = std::max(1e-6f, seconds);
        mSeg.length(mLength);
    }

    void setFreq(float hz)
    {
        mSeg.freq(std::max(0.f, hz));
    }

    void setPhase(float phase)
    {
        mSeg.phase(std::clamp(phase, 0.f, 0.999999f));
    }

    void setLevels(float start, float end)
    {
        mStart = start;
        mEnd   = end;
        mSeg.levels(mStart, mEnd);
    }

    void setEnd(float end)
    {
        mEnd = end;
        mSeg = end;  // Gamma: smooth transition from current value
    }

    void reset()
    {
        mSeg.reset();
    }

    bool done() const
    {
        return mSeg.done();
    }

    float value() const
    {
        return mSeg.value();
    }

    float process() override
    {
        return mSeg();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::Seg<double> mSeg;

    float mLength;
    float mStart;
    float mEnd;
};


class SegExp : public Generator {
public:
    SegExp(float lengthSeconds = 0.1f,
           float curvature = -3.f,
           float start = 0.f,
           float end   = 1.f)
    : 
    mSeg(lengthSeconds, curvature, start, end),
    mLength(lengthSeconds),
    mCurve(curvature),
    mStart(start),
    mEnd(end)  
    {
        
    }

    void set(float lengthSeconds, float curvature)
    {
        mLength = std::max(1e-6f, lengthSeconds);
        mCurve  = curvature;
        mSeg.set(mLength, mCurve);
    }

    void setLength(float seconds)
    {
        mLength = std::max(1e-6f, seconds);
        mSeg.length(mLength);
    }

    void setCurve(float curvature)
    {
        mCurve = curvature;
        mSeg.curve(mCurve);
    }

    void setLevels(float start, float end)
    {
        mStart = start;
        mEnd   = end;
        mSeg.levels(mStart, mEnd);
    }

    void setEnd(float end)
    {
        mEnd = end;
        mSeg = end;
    }

    void reset()
    {
        mSeg.reset();
    }

    bool done() const
    {
        return mSeg.done();
    }

    float value() const
    {
        return mSeg.value();
    }

    float process()
    {
        return mSeg();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::SegExp<double, double> mSeg;

    float mLength;
    float mCurve;
    float mStart;
    float mEnd;
};

class SilenceDetect : public Function {
public:

    SilenceDetect(unsigned count = 1000)    
    : mDetect(count)
    {
    }

    void setCount(unsigned count)
    {
        mDetect.count(count);
    }

    void reset()
    {
        mDetect.reset();
    }

    float process(float input) override
    {
        return (float)mDetect(input, threshold);
    }

    bool done() const
    {
        return mDetect.done();
    }

    void run(const float* input, bool* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    void setThreshold(float v) { threshold = v; }


    float threshold = 0.001f;
    uint32_t count  = 1000;
    
private:
    gam::SilenceDetect mDetect;
    
};



class Sine : public Generator
{
public:
    Sine(float f)
    {
        setFreq(f);        
    }

    void setFreq(float f)
    {        
        mOsc.freq(f);
    }

    float process() override
    {
        return mOsc();
    }

private:
    gam::Sine<double> mOsc;
};

class SineD : public Generator {
public:
    SineD(float freq = 440.0,
          float amp  = 1.0,
          float decay60 = -1.0,
          float phase = 0.0)
    : mOsc(freq, amp, decay60, phase)
    {
        
    }




    void set(float f, float a, float d, float p=0.0)
    {
        mOsc.set(f, a, d, p);
    }

    void setFreq(float f)
    {
        mOsc.freq(f);
    }

    void setPeriod(float seconds)
    {
        if(seconds > 0.0)
            mOsc.period(seconds);
    }

    void setAmp(float a)
    {
        mOsc.ampPhase(a, 0.0);
    }

    void setDecay(float d)
    {
        mOsc.decay(d);
    }

    void setPhase(float p)
    {
        mOsc.ampPhase(amp(), p);
    }

    float freq() const
    {
        return mOsc.freq();
    }

    float amp() const
    {
        return mOsc.val;
    }

    float process() override
    {
        return mOsc();
    }

    void reset()
    {
        mOsc.ampPhase(amp(), 0.0);
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }


private:
    gam::SineD<double> mOsc;
};

class SineDs : public Generator {
public:
    
    SineDs(unsigned count = 1)
    : mBank(count)
    {
        
    }

    void resize(unsigned count)
    {
        mBank.resize(count);
    }

    void set(unsigned index,
                    float freq,
                    float amp,
                    float decay60,
                    float phase=0.0f)
    {
        if(index < mBank.size())
            mBank.set(index, freq, amp, decay60, phase);
    }

    float process() override
    {
        return mBank();
    }

    float last(unsigned index) const
    {
        if(index < mBank.size())
            return mBank.last(index);
        return 0.0;
    }

    unsigned size() const
    {
        return mBank.size();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }


private:
    gam::SineDs<double> mBank;
};

class SineR : public Generator {
public:
    SineR(float freq = 440.0,
          float amp  = 1.0,
          float phase = 0.0)
    : mOsc(freq, amp, phase)
    {

    }

    void set(float f, float a, float p=0.0f)
    {
        mOsc.set(f, a, p);
    }

    void setFreq(float f)
    {
        mOsc.freq(f);
    }

    void setPeriod(float seconds)
    {
        if(seconds > 0.0)
            mOsc.period(seconds);
    }

    void setAmp(float a)
    {
        mOsc.ampPhase(a, 0.0);
    }

    void setPhase(float p)
    {
        mOsc.ampPhase(amp(), p);
    }

    float freq() const
    {
        return mOsc.freq();
    }

    float amp() const
    {
        return mOsc.val;
    }
    float process()
    {
        return mOsc();
    }

    void reset()
    {
        mOsc.ampPhase(amp(), 0.0);
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::SineR<double> mOsc;
};


class SineRs : public Generator {
public:
    SineRs(unsigned count = 1)
    : mBank(count)
    {
        
    }

    void resize(unsigned count)
    {
        mBank.resize(count);
    }

    void set(unsigned index, float freq, float amp=1.0f, float phase=0.0f)
    {
        if(index < mBank.size())
            mBank.set(index, freq, amp, phase);
    }

    float process()
    {
        return mBank();
    }

    float last(unsigned index) const
    {
        if(index < mBank.size())
            return mBank.last(index);
        return 0.0;
    }

    unsigned size() const
    {
        return mBank.size();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::SineRs<double> mBank;
};

class SOS : public Function {
public:
    SOS(unsigned sections)
    {
        mSections.reserve(sections);
        for (unsigned i = 0; i < sections; ++i)
            mSections.emplace_back((float)gam::sampleRate());
    }

    void setFreq(float f) {
        float freq = (std::clamp(f, 5.f, 0.45f * (float)gam::sampleRate()));
        for (auto& sec : mSections) {
            sec.setFreq(freq);
        }
    }

    void setRes(float r) {
        float f = std::clamp(r, 0.001f, 50.f);
        for (auto& sec : mSections) {
            sec.setRes(f);
        }    
    }

    void setLevel(float l) {
        float f = std::max(0.f, l);
        for (auto& sec : mSections) {
            sec.setLevel(f);
        }
    }

    void reset()
    {
        for(size_t i = 0; i < mSections.size(); i++) mSections[i].reset();
    }
    float process(float input) override
    {    
        float x = input;
        for (auto& sec : mSections) {
            x = sec.process(x);
        }
        return x;
    }
    void run(const float* in, float* out, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }
    void setSection(size_t i, float b0, float b1, float b2, float a1, float a2)
    {
        mSections[i].setCoef(b0,b1,b2,a1,a2);
    }

    std::vector<Biquad> mSections;    
};

class Square : public Generator {
public:
    Square(float freq = 440.f,
           float phase = 0.f)
    : mOsc(freq, phase)
    {
        
    }

    void setFreq(float f)
    {
        mOsc.freq(f);
    }

    void setPeriod(float seconds)
    {
        if(seconds > 0.f)
            mOsc.freq(1.f / seconds);
    }

    float process()
    {
        return mOsc(integrate);
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::Square<double> mOsc;
    float integrate = 0.999f;
};

class Threshold: public Function {
public:
    Threshold(float threshold = 0.5f, float smoothFreq = 10.0f)
    : 
    mValue(0.0f),
    mThresh(threshold, smoothFreq)
    {
    }

    void setThreshold(float v)
    {
        mThresh.thresh = v;
    }

    void setSmoothFreq(float freqHz)
    {
        mThresh.lpf.freq(std::max(0.01f, freqHz));
    }

    void reset(float value=0.0f)
    {
        mValue = value;
        mThresh.lpf.reset(value);
    }

    float process(float input) override
    {
        mValue = mThresh(input);
        return mValue;
    }

    float process(float input, float highValue, float lowValue)
    {
        mValue = mThresh(input, highValue, lowValue);
        return mValue;
    }

    float processInv(float input)
    {
        mValue = mThresh.inv(input);
        return mValue;
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    
    
    float mThreshold = 0.5f;
    float mSmooth    = 0.5f;    
    float mValue     = 0.0f;   // cached last output

private:
    gam::Threshold<double> mThresh;
    
};

class ZeroCross: public Function {
public:
    ZeroCross(float initialValue = 0.0f)
    : mDetector(initialValue)
    {
    }

    // Process one sample
    // Returns:  1  → rising zero-cross
    //           0  → no crossing
    //          -1  → falling zero-cross
    
    float process(float input) override
    {
        return (float)mDetector(input);
    }

    void run(const float* input, int* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    
private:
    gam::ZeroCross<double> mDetector;
};

class ZeroCrossRate: public Function {
public:
    ZeroCrossRate(unsigned windowSize = 256)
    : mZCR(windowSize)
    {
    }

    void reset()
    {
        mZCR.reset();
    }

    void setWindow(unsigned windowSize)
    {
        mZCR.period(windowSize);
    }

    float process(float input) override
    {
        return mZCR(input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    float value() const
    {
        return mZCR.value();
    }


private:
    gam::ZeroCrossRate<double> mZCR;
};


struct ModGenerator : public Generator
{
    virtual float process() = 0;

    void setFM(float f)    { fm.set(f); }
    void setPM(float f)    { pm.set(f); }
    void setAM(float f)    { am.set(f); }
    void setRatio(float f){ ratio.set(f); }
    void setIndex(float f){ index.set(f); }

    float baseFreq = 440.f;   // 🔑 the real carrier frequency
    Modulator fm, pm, am, ratio, index;

protected:
    
    template<class Osc>
    float update(Osc& osc)
    {
        float _fm    = fm.process();
        float _pm    = pm.process();
        float _am    = am.process();
        float r      = ratio.process();
        float I      = index.process();

        float fc   = baseFreq;        // stable carrier
        float fmHz = fc * r;
        float df   = _fm * fmHz * I;

        osc.phaseAdd(_pm);
        osc.freq(fc + df);
        float y = osc();
        osc.phaseAdd(-_pm);

        return y * _am;
    }
};


struct ModFilter1 : public Function
{
    virtual float process(float input) = 0;

    void setCutoff(float f) { baseCutoff = f; }
    void setFM(float f)     { cutoff.set(f); }
    void setAM(float a)     { am.set(a); }

protected:
    float baseCutoff = 1000.0f;
    
    Modulator cutoff;
    Modulator am;

    template<class Filter>
    float update(Filter& filter, float input)
    {
        float _cut  = cutoff.process();   // [-1,1]        
        float _am   = am.process();       // [0,1] or similar

        float fc  = baseCutoff + baseCutoff * _cut;
     
        float sr = gam::sampleRate();
        fc = std::clamp(fc, 0.001f*sr, 0.45f*sr);
     
        filter.freq(fc);
        
        return filter(input) * _am;
    }
};

struct ModFilter2 : public Function
{
    virtual float process(float input) = 0;

    void setCutoff(float f) { baseCutoff = f; }
    void setWidth(float f)  { baseWidth = f; }
    void setFM(float f)     { cutoff.set(f); }
    void setWM(float w)     { width.set(w); }
    void setAM(float a)     { am.set(a); }

protected:
    float baseCutoff = 1000.0f;
    float baseWidth  = 0.0f;
    
    Modulator cutoff;
    Modulator width;
    Modulator am;

    template<class Filter>
    float update(Filter& filter, float input)
    {
        float _cut  = cutoff.process();   // [-1,1]
        float _wid  = width.process();
        float _am   = am.process();       // [0,1] or similar

        float fc  = baseCutoff + baseCutoff * _cut;
        float w   = baseWidth  + baseWidth  * _wid;

        fc = std::max(fc, 5.0f);          // safety
        
        filter.freq(fc);
        filter.width(w);
        return filter(input) * _am;
    }
};


struct ModFilter : public Function
{
    virtual float process(float input) = 0;

    void setCutoff(float f) { baseCutoff = f; }
    void setRes(float f)    { baseRes = f; }
    void setFM(float f)     { cutoff.set(f); }
    void setQM(float r)     { res.set(r); }
    void setAM(float a)     { am.set(a); }
    void setIM(float a)     { im.set(a); }
    void setFBM(float a)     { fb.set(a); }

protected:
    float baseCutoff = 1000.0f;
    float baseRes    = 0.707f;
    float last_y     = 0.0f;

    Modulator cutoff;
    Modulator res;
    Modulator am;
    Modulator im;    
    Modulator fb;

    template<class Filter>
    float update(Filter& filter, float input)
    {
        float _cut  = cutoff.process();   // [-1,1]
        float _res  = res.process();      // [-1,1]
        float _am   = am.process();       // [0,1] or similar
        float _im   = im.process();        
        float _fb   = fb.process();

        float fc  = baseCutoff + baseCutoff * _cut;
        float q   = baseRes    + baseRes    * _res;

        float sr = (float)gam::sampleRate();
        fc = std::clamp(fc, 0.001f * sr, 0.45f * sr);
        q  = std::max(q, 0.001f);

        filter.freq(fc);
        filter.res(q);

        float y = filter(_im * input + _fb * last_y) * _am;
        last_y = y;
        return y;
    }
};


class ModDelay : public Function
{
public:
    ModDelay(float maxDelay=2.0f, float initDelay=0.5f) : _delay(maxDelay,initDelay) {
        setDM(0.0f);
        setFBM(0.0f);
        setMIXM(0.5f);
        setIM(0.0f);        
        setAM(0.0f);
        setDelay(initDelay);
        setFeedback(0.0f);
        setMix(0.5f);
        setIpolType(gam::ipl::Type::CUBIC);
    }
    virtual ~ModDelay() {
        
    }

    
    void setDM(float v) { dm.set(v); }      // delay time mod
    void setFBM(float v) { fbm.set(v); }    // feedback mod
    void setMIXM(float v) { mixm.set(v); }  // mix mod
    void setIM(float v) { im.set(v); }      // input am    
    void setAM(float v) { am.set(v); }      // output am

    void  setDelay(float d) { mBaseDelay = d; _delay.setDelay(d); }
    void  setFeedback(float f) { mBaseFeedback = f; }
    void  setMix(float m) { mBaseMix = m; }
    void  setIpolType(gam::ipl::Type type) { _delay.setIpolType(static_cast<gam::ipl::Type>(type)); }
    float getDelay() const { return mBaseDelay; }
    float getMaxDelayTime() const { return _delay.getMaxDelayTime(); }
    
    virtual float process(float input) override
    {        
        float delayT   = dm.process(mBaseDelay);        
        float mix      = mixm.process(mBaseMix);

        delayT   = std::clamp(delayT, 0.0001f,_delay.getMaxDelayTime());
        
        mix      = std::clamp(mix, 0.0f, 1.0f);

        _delay.setDelay(delayT);
        
        float d = _delay.read();
        _delay.write(im.process(input) + fbm.process(d * mBaseFeedback));

        return am.process(input * (1.0f - mix) + d * mix);
    }

    float read() const { return _delay.read(); }
    float read(float ago) const { return _delay.read(ago); }
    void  write(const float& v) { _delay.write(v); }


    Modulator dm, fbm, mixm, im, am;
    float mBaseDelay = 0.5f;
    float mBaseFeedback = 0.0f;
    float mBaseMix = 1.0f;
    

protected:
    
    Delay _delay;
};

class ModComb2 : public Function
{
public:
    ModComb2(float maxDelay = 2.0f, float initDelay = 0.05f)
    : _comb(maxDelay, initDelay)
    {
        baseDelay     = initDelay;
        baseFeedback  = 0.0f;
        baseFF        = 0.0f;
        baseAP        = 0.0f;
        baseMix       = 0.5f;

        setAM(0.0f);
        setIM(0.0f);
    }

    // ---------------- Base controls ----------------
    void setDelay(float d)        { baseDelay = d; }
    void setFeedback(float v)     { baseFeedback = v; }
    void setFeedforward(float v)  { baseFF = v; }
    void setAllPass(float v)      { baseAP = v; }
    void setMix(float v)          { baseMix = v; }

    // ---------------- Modulation depths ----------------
    void setDM(float v)   { dm.set(v); }
    void setFBM(float v)  { fm.set(v); }
    void setFFM(float v)  { ff.set(v); }
    void setAPM(float v)  { ap.set(v); }
    void setMIXM(float v) { mm.set(v); }

    // ---------------- Gain stages ----------------
    void setAM(float v) { am.set(v); }
    void setIM(float v) { im.set(v); }

    float process(float input) override
    {
        float _dm  = dm.process(baseDelay);
        float _fb  = fm.process(baseFeedback);
        float _ff  = ff.process(baseFF);
        float _ap  = ap.process(baseAP);
        float _mix = mm.process(baseMix);

        
        float delay    = std::clamp(_dm, 0.0001f, _comb.getMaxDelayTime());
        float feedback = std::clamp(_fb, -0.999f, 0.999f);
        float ffwd     = std::clamp(_ff, -0.999f, 0.999f);
        float allpass  = std::clamp(_ap, -0.999f, 0.999f);
        float mix      = std::clamp(_mix, 0.0f, 1.0f);

        _comb.setDelay(delay);
        _comb.setFeedback(feedback);
        _comb.setFeedforward(ffwd);
        _comb.setAllPass(allpass);

        float y = _comb.process(im.process(input));

        float out = input * (1.0f - mix) + y * mix;

        return am.process(out);
    }

private:
    Comb _comb;

    float baseDelay;
    float baseFeedback;
    float baseFF;
    float baseAP;
    float baseMix;

    Modulator dm, fm, ff, ap, mm;
    Modulator am, im;
};

struct Tap
{
    float     baseDelay = 0.25f;   // seconds
    Modulator fbm;                 // feedback modulation
    Modulator mixm;                // tap level modulation
    float     lastOut = 0.0f;
};

class ModMultitap : public Function
{
public:

    ModMultitap(unsigned numTaps = 4, float maxDelaySeconds = 2.0f)
    : mDelay(maxDelaySeconds, numTaps)
    {
        setNumTaps(numTaps);
        setGlobalMix(1.0f);
        setAM(0.0f);
    }

    void setNumTaps(unsigned n)
    {
        n = std::max(1u, n);
        taps.resize(n);
        mDelay.setNumTaps(n);
    }

    unsigned getNumTaps() const { return (unsigned)taps.size(); }

    void setTapDelay(unsigned i, float sec)
    {
        if (i >= taps.size()) return;
        taps[i].baseDelay = std::max(sec, 0.0001f);
    }

    void setGlobalMix(float v) { mGlobalMix = std::clamp(v, 0.f, 1.f); }

    void setFBM(unsigned i, float v)
    {
        if (i >= taps.size()) return;
        taps[i].fbm.set(v);
    }

    void setMIXM(unsigned i, float v)
    {
        if (i >= taps.size()) return;
        taps[i].mixm.set(v);
    }

    void setAM(float v) { am.set(v); }

    float process(float input) override
    {
        float fbSum  = 0.0f;
        float wetSum = 0.0f;
        float mixSum = 0.0f;

        for (unsigned i = 0; i < taps.size(); ++i)
        {
            auto& t = taps[i];

            // 🔧 THIS WAS THE MISSING PIECE
            mDelay.setTapTime(i, t.baseDelay);

            float fb  = std::clamp(t.fbm.process(), -0.999f, 0.999f);
            float lvl = std::clamp(t.mixm.process(),  0.0f,  1.0f);

            float d = mDelay.read(i);

            fbSum  += d * fb;
            wetSum += d * lvl;
            mixSum += lvl;

            t.lastOut = d;
        }

        mDelay.write(input + fbSum);

        float wet = (mixSum > 1e-6f) ? (wetSum / mixSum) : 0.0f;
        float out = input * (1.0f - mGlobalMix) + wet * mGlobalMix;

        return out * am.process();
    }

    void run(const float* in, float* out, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }

private:
    MultitapDelay mDelay;
    std::vector<Tap> taps;
    float mGlobalMix = 1.0f;
    Modulator am;
};

class ModAllPass1 : public ModFilter1
{
public:

    ModAllPass1(float initFreq = 1000.0f)
    {        
        setCutoff(initFreq);
        setAM(0.0f);
    }   

    float process(float input) override
    {
        return update(mFilter, input);
    }
    
    void reset()        { mFilter.reset(); }

    float high(float x) { return mFilter.high(x); }
    float low (float x) { return mFilter.low(x); }

    float getFreq() { return mFilter.freq(); }

private:
    
    gam::AllPass1<double> mFilter;
};

class ModAllPass2 : public ModFilter2
{
public:
    ModAllPass2(float initFreq = 1000.0f, float initWidth = 100.0f)
    {
        setCutoff(initFreq);
        setWidth(initWidth);
        setAM(0.0f);
    }

    float process(float input) override
    {
        // reuse ModFilter's modulation & safety path
        return update(mFilter, input);
    }

    
    void reset() { mFilter.reset(); }

private:
    gam::AllPass2<double> mFilter;
};

class ModAM : public Function
{
public:
    ModAM(float depth = 1.0f)
    : mAM(depth)
    {
        setDepth(1.0f);
    }

    // -------- Modulation controls --------
    void setMM(float v)    { mm.set(v); }   // mod input
    void setDM(float d)    { dm.set(d); }      // depth mod
    void setDepth(float f) { mAM.setDepth(f); dm.set(f); }   

    float process(float carrier)
    {
        mAM.setDepth(dm.process());
        float y = mAM.process(carrier, mm.process());
        return y;
    }

private:
    AM mAM;    
    Modulator dm;
    Modulator mm;
};

class ModBiquad : public ModFilter
{
public:
    using FilterType = gam::FilterType;

    ModBiquad(float initFreq = 1000.0f,
           float initRes  = 0.707f,
           FilterType type = gam::LOW_PASS,
           float initLevel = 1.0f)
    {
        setCutoff(initFreq);
        setRes(initRes);
        mFilter.level(initLevel);
        setAM(0.0f);
        setIM(0.0f);
        setFBM(0.0f);
    }

    void setType(FilterType t){ mFilter.type(t); }
    void setCoef(float a0, float a1, float a2, float b1, float b2) { mFilter.coef(a0, a1, a2, b1, b2); }
    void reset() { mFilter.reset(); }
    
    float getFreq()  const { return baseCutoff; }
    float getRes()   const { return baseRes; }
    float getLevel() const { return mFilter.level(); }

    float process(float input)
    {
        return update(mFilter, input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    
private:
    gam::Biquad<double> mFilter;
};

class ModBiquad3 : public Function {
public:
    using FilterType = gam::FilterType;

    ModBiquad3(float f0, float f1, float f2,
               float Q = 1.0f,
               FilterType t0 = FilterType::LOW_SHELF,
               FilterType t1 = FilterType::PEAKING,
               FilterType t2 = FilterType::HIGH_SHELF,
               float level = 1.0f)
    : mFilter(f0,f1,f2,Q),
      last_y(0.0f)
    {
        mFilter.setType0(t0);
        mFilter.setType1(t1);
        mFilter.setType2(t2);
        mFilter.setLevel(level);

        baseF0 = f0;
        baseF1 = f1;
        baseF2 = f2;
        baseQ  = Q;

        am.set(1.0f);
        im.set(1.0f);        
        fb.set(0.0f);
    }

    // -------- Base parameter controls (slow knobs) --------

    void setCutoff0(float f) { baseF0 = f; }
    void setCutoff1(float f) { baseF1 = f; }
    void setCutoff2(float f) { baseF2 = f; }
    void setQ(float q)       { baseQ  = q; }

    void setType0(gam::FilterType type) { mFilter.setType0(type); }
    void setType1(gam::FilterType type) { mFilter.setType1(type); }
    void setType2(gam::FilterType type) { mFilter.setType2(type); }

    // -------- Modulation depths --------

    void setFM0(float v) { fm0.set(v); }
    void setFM1(float v) { fm1.set(v); }
    void setFM2(float v) { fm2.set(v); }
    void setQM(float v)  { qm.set(v);  }

    void setDepthF0(float v) { depthF0 = v; }
    void setDepthF1(float v) { depthF1 = v; }
    void setDepthF2(float v) { depthF2 = v; }

    void setAM(float v)  { am.set(v); }
    void setIM(float v)  { im.set(v); }    
    void setFB(float v)  { fb.set(v); }

    // -------- Audio processing --------

    float process(float input) override
    {
        const float _f0  = fm0.process();
        const float _f1  = fm1.process();
        const float _f2  = fm2.process();
        const float _q   = qm.process();
        const float _am  = am.process();
        const float _im  = im.process();        
        const float _fb  = fb.process();

        float f0 = baseF0 + depthF0 * baseF0 * _f0;
        float f1 = baseF1 + depthF1 * baseF1 * _f1;
        float f2 = baseF2 + depthF2 * baseF2 * _f2;

        float q  = baseQ  + (baseQ * depthQ * _q);

        float sr = (float)gam::sampleRate();

        f0 = std::clamp(f0, 5.0f, 0.45f * sr);
        f1 = std::clamp(f1, 5.0f, 0.45f * sr);
        f2 = std::clamp(f2, 5.0f, 0.45f * sr);
        q  = std::max(q, 0.001f);

        mFilter.setFreqs(f0, f1, f2);
        mFilter.setRes(q);

        float y = mFilter.process(_im * input + _fb * last_y);
        y *= _am;

        last_y = y;
        return y;
    }

private:
    Biquad3 mFilter;

    // Base parameters
    float baseF0 = 400.0f;
    float baseF1 = 1200.0f;
    float baseF2 = 2800.0f;
    float baseQ  = 1.0f;

    float depthF0 = 1.0f;
    float depthF1 = 1.0f;
    float depthF2 = 1.0f;
    float depthQ  = 1.0f;

    // Modulators
    Modulator fm0, fm1, fm2, qm;
    Modulator am, im, fb;

    float last_y;
};

class ModBuzz : public ModGenerator
{
public:
    ModBuzz(float freq = 440.f,
            float phase = 0.f,
            float harmonics = 8.f)
    {
        setFreq(freq);
        setHarmonics(harmonics);

        setFM(0.f);
        setPM(0.f);
        setAM(1.f);
        setRatio(1.f);
        setIndex(1.f);
    }

    void setFreq(float f)
    {
        baseFreq = f;            // 🔑 required for modulation math
        osc.freq(f);
    }

    void setPeriod(float seconds)
    {
        if (seconds > 0.f)
            setFreq(1.f / seconds);
    }

    void setHarmonics(float h)
    {
        osc.harmonics(h);
    }

    void setHarmonicsMax()
    {
        osc.harmonicsMax();
    }

    void antialias()
    {
        osc.antialias();
    }

    void normalize(bool enable)
    {
        osc.normalize(enable);
    }

    float process() override
    {
        return update(osc);
    }

private:
    gam::Buzz<double> osc;
};

class ModCeleste : public Function
{
public:
    ModCeleste(float delay = 0.04f,
               float amount = 0.004f,
               float freq = 0.6f)
    : baseDelay(delay),
      baseAmount(amount),
      baseFreq(freq)
    {
        setIM(0.0f);
        setAM(0.0f);
        setMix(0.5f);
    }

    // ---------- Base controls ----------
    void setDelay(float v)  { baseDelay = v; }
    void setAmount(float v) { baseAmount = v; }
    void setFreq(float v)   { baseFreq = v; }
    void setMix(float v)    { baseMix = std::clamp(v, 0.0f, 1.0f); }

    // ---------- Modulation depths ----------
    void setDM(float v)   { dm.set(v); }
    void setAMM(float v)  { amod.set(v); }
    void setFM(float v)   { fm.set(v); }
    void setMIXM(float v) { mixm.set(v); }

    // ---------- Gain stages ----------
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float input) override
    {
        float _dm   = dm.process();
        float _amod = amod.process();
        float _fm   = fm.process();
        float _mixm = mixm.process();

        float delay  = baseDelay  + baseDelay  * _dm;
        float amount = baseAmount + baseAmount * _amod;
        float freq   = baseFreq   + baseFreq   * _fm;
        float mix    = baseMix    + baseMix    * _mixm;

        delay  = std::max(0.0f, delay);
        amount = std::max(0.0f, amount);
        freq   = std::max(0.01f, freq);
        mix    = std::clamp(mix, 0.0f, 1.0f);

        mCeleste.delay(delay);
        mCeleste.amount(amount);
        mCeleste.freq(freq);

        float y = mCeleste(input * im.process());

        float out = input * (1.0f - mix) + y * mix;
        return out * am.process();
    }

private:
    gam::Celeste<double> mCeleste;

    float baseDelay;
    float baseAmount;
    float baseFreq;
    float baseMix = 0.5f;

    Modulator dm;     // delay modulation
    Modulator amod;   // amount modulation
    Modulator fm;     // frequency modulation
    Modulator mixm;   // wet/dry modulation
    Modulator im;
    Modulator am;
};

class ModChirp : public Generator
{
public:
    ModChirp(float startFreq = 220.f,
             float endFreq   = 0.f,
             float decay60   = 0.2f)
    : baseStart(startFreq),
      baseEnd(endFreq),
      baseDecay(decay60),
      mChirp(startFreq, endFreq, decay60)
    {
        setAM(0.0f);
    }

    // -------- Base controls --------
    void setStart(float f) { baseStart = f; }
    void setEnd(float f)   { baseEnd   = f; }
    void setDecay(float d) { baseDecay = d; }

    // -------- Modulation depths --------
    void setSM(float v) { sm.set(v); }   // start freq mod
    void setEM(float v) { em.set(v); }   // end freq mod
    void setDM(float v) { dm.set(v); }   // decay mod

    // -------- Output gain --------
    void setAM(float v) { am.set(v); }

    void reset()
    {
        mChirp.reset();
    }

    float process() override
    {
        float _sm = sm.process();
        float _em = em.process();
        float _dm = dm.process();

        float start = baseStart + baseStart * _sm;
        float end   = baseEnd   + baseEnd   * _em;
        float decay = baseDecay + baseDecay * _dm;

        start = std::max(start, 0.1f);
        end   = std::max(end,   0.1f);
        decay = std::max(decay, 0.001f);

        mChirp.freq(start, end);
        mChirp.decay(decay);

        return mChirp() * am.process();
    }

private:
    float baseStart;
    float baseEnd;
    float baseDecay;

    gam::Chirp<double> mChirp;

    Modulator sm;  // start freq mod
    Modulator em;  // end freq mod
    Modulator dm;  // decay mod
    Modulator am;
};

class ModDSF : public ModGenerator
{
public:
    ModDSF(float freq = 440.f,
           float freqRatio = 1.f,
           float ampRatio = 0.5f,
           float harmonics = 8.f)
    {
        setFreq(freq);
        setFreqRatio(freqRatio);
        setAmpRatio(ampRatio);
        setHarmonics(harmonics);

        setFM(0.f);
        setPM(0.f);
        setAM(1.f);
        setRatio(1.f);
        setIndex(1.f);
    }

    void setFreq(float f)
    {
        baseFreq = f;          // 🔑 required for modulation math
        osc.freq(f);
    }

    void setPeriod(float seconds)
    {
        if (seconds > 0.f)
            setFreq(1.f / seconds);
    }

    void setFreqRatio(float r)
    {
        osc.freqRatio(r);
    }

    void setAmpRatio(float r)
    {
        osc.ampRatio(r);
    }

    void setHarmonics(float h)
    {
        osc.harmonics(h);
    }

    void setHarmonicsMax()
    {
        osc.harmonicsMax();
    }

    void antialias()
    {
        osc.antialias();
    }

    float process() override
    {
        return update(osc);
    }

private:
    gam::DSF<double> osc;
};

class ModFM : public Function
{
public:
    ModFM(float maxDelay = 0.05f, float baseDelay = 0.005f)
    : delay(maxDelay), baseDelay(baseDelay)
    {
        setDepth(0.0f);
        setFB(0.0f);
        setIM(0.0f);
        setAM(0.0f);
    }

    // -------- Controls --------
    void setBaseDelay(float d) { baseDelay = std::max(d, 0.0f); }
    void setDepth(float d)     { depth = std::max(d, 0.0f); }
    void setFB(float f)        { fb = std::clamp(f, 0.0f, 0.99f); }

    void setMM(float v) { mm.set(v); }   // modulation signal
    void setIM(float v) { im.set(v); }   // input gain
    void setAM(float v) { am.set(v); }   // output gain

    float process(float input)
    {
        float m  = mm.process();           // [-1,1]
        float in = input * im.process();

        float delayTime = baseDelay + depth * m;
        delayTime = std::clamp(delayTime, 0.0f, delay.maxDelay());

        delay.delay(delayTime);

        float y = delay(in + fb * last);

        last = y;
        return y * am.process();
    }

private:
    gam::Delay<float> delay;

    float baseDelay;
    float depth = 0.0f;
    float fb    = 0.0f;

    Modulator mm;
    Modulator im;
    Modulator am;

    float last = 0.0f;
};

class ModFreqShift : public Function
{
public:
    ModFreqShift(float shiftHz = 0.0f)
    : mShift(shiftHz),
      baseShift(shiftHz)
    {
        setIM(0.0f);
        setAM(0.0f);
        setMix(1.0f);     // default fully wet (freq shift is usually used wet)
        setFB(0.0f);
        setShiftDepth(0.0f);
    }

    // ---------------- Base controls ----------------
    void setShift(float hz) { baseShift = hz; }
    void setShiftDepth(float d) { depthShift = std::max(0.0f, d); } // in "fraction of base" or absolute? (see below)
    void setFB(float v) { baseFB = std::clamp(v, -0.999f, 0.999f); }
    void setMix(float v) { baseMix = std::clamp(v, 0.0f, 1.0f); }

    // ---------------- Modulation inputs ----------------
    // Mod signals should be in [-1,1] unless noted.
    void setSM(float v)   { sm.set(v); }    // shift modulation signal
    void setFBM(float v)  { fbm.set(v); }   // feedback modulation signal
    void setMIXM(float v) { mixm.set(v); }  // mix modulation signal

    // ---------------- Gain stages ----------------
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    // Optional: clamp shift to avoid insane settings
    void setMaxAbsShift(float hz) { maxAbsShift = std::max(0.0f, hz); }

    float process(float input) override
    {
        const float _im   = im.process();
        const float _am   = am.process();

        const float _sm   = sm.process();     // [-1,1]
        const float _fbm  = fbm.process();    // [-1,1]
        const float _mixm = mixm.process();   // [-1,1]

        // --- Compute modulated parameters ---
        // Shift modulation model:
        // shift = baseShift + (depthShift * baseShift) * sm
        // This mirrors your preferred "base + depth*base*signal" style.
        float shift = baseShift + (depthShift * baseShift) * _sm;

        if (maxAbsShift > 0.0f)
            shift = std::clamp(shift, -maxAbsShift, maxAbsShift);

        // Feedback modulation model: base + base*modSignal
        float fb = baseFB + baseFB * _fbm;
        fb = std::clamp(fb, -0.999f, 0.999f);

        // Mix modulation model: base + base*modSignal
        float mix = baseMix + baseMix * _mixm;
        mix = std::clamp(mix, 0.0f, 1.0f);

        // --- Apply shift ---
        // Update underlying FreqShift parameter per-sample (OK for audio-rate modulation).
        mShift.setShift(shift);

        // --- Feedback topology (1-sample delayed feedback) ---
        float in = input * _im + lastY * fb;

        float wet = mShift.process(in);

        // --- Wet/dry mix & output gain ---
        float out = input * (1.0f - mix) + wet * mix;
        out *= _am;

        lastY = wet; // feedback uses wet path (usually best for freq shifting)
        return out;
    }

private:
    FreqShift mShift;

    // Base params
    float baseShift   = 0.0f;  // Hz
    float depthShift  = 0.0f;  // unitless depth (0..inf, typically 0..1)
    float baseFB      = 0.0f;
    float baseMix     = 1.0f;

    // Safety
    float maxAbsShift = 0.0f;  // 0 disables clamp

    // Modulators (signals)
    Modulator sm;    // shift mod signal
    Modulator fbm;   // feedback mod signal
    Modulator mixm;  // mix mod signal
    Modulator im;    // input gain
    Modulator am;    // output gain

    float lastY = 0.0f;
};

class ModLFO : public ModGenerator
{
public:
    ModLFO(float freq = 1.f, float phase = 0.f, float mod = 0.5f)
    {
        setFreq(freq);
        lfo.setPhase(phase);
        lfo.setMod(mod);

        setFM(0.0f);
        setPM(0.0f);
        setAM(0.0f);
        setRatio(1.0f);
        setIndex(1.0f);
    }

    void setFreq(float f)
    {
        baseFreq = std::max(0.001f, f);
        lfo.setFreq(baseFreq);
    }

    void setMod(float m)
    {
        lfo.setMod(std::clamp(m, 0.0f, 1.0f));
    }

    void setWave(LFO::Wave w) { lfo.setWave(w); }
    void setUnipolar(bool v)  { lfo.setUnipolar(v); }

    float process() override
    {
        return update(lfo);
    }

private:
    LFO lfo;
};

class ModNotch : public ModFilter2
{
public:
    ModNotch(float initFreq = 1000.0f, float initWidth = 100.0f)
    {
        setCutoff(initFreq);
        setWidth(initWidth);
        setAM(0.0f);
    }

    float process(float input) override
    {
        return update(mFilter, input);
    }

    void reset() { mFilter.reset(); }

private:
    gam::Notch<double> mFilter;
};

class ModOnePole : public ModFilter1
{
public:

    ModOnePole(float initFreq = 1000.0f,
               gam::FilterType type = gam::FilterType::LOW_PASS,
               float stored = 0.0f)
    {
        setCutoff(initFreq);
        setAM(0.0f);
        setType(type);        
    }

    void setType(gam::FilterType t) { mFilter.type(t); }

    void lag(float seconds, float threshold = 0.001f)
    {
        mFilter.lag(seconds, threshold);
    }

    float process(float input) override
    {
        return update(mFilter, input);
    }

    float last() const { return mFilter.last(); }

private:
    gam::OnePole<double> mFilter;
};

class ModOsc : public ModGenerator {
public:
    ModOsc(float freq = 440.f,
        float phase = 0.f,
        unsigned tableSize = 512)
    : mOsc(freq, phase, tableSize)
    {
    
    }

    void setFreq(float f) { mOsc.freq(f); } 
    void setPeriod(float seconds) {
        if(seconds > 0.f)
            mOsc.freq(1.f / seconds);
    }   
    void setPhase(float p) { mOsc.phase(p); }
    float freq() const { return mOsc.freq(); }
    float phase() const { return mOsc.phase(); }
    void clearTable()
    {
        for(unsigned i = 0; i < mOsc.size(); ++i)
            mOsc[i] = 0.f;
    }
    void addSine(float cycles, float amplitude, float phase)
    {
        mOsc.addSine(cycles, amplitude, phase);
    }
    float getTableSample(unsigned index) const
    {
        if(index < mOsc.size())
            return mOsc[index];
        return 0.f;
    }

    void setTableSample(unsigned index, float value)
    {
        if(index < mOsc.size())
            mOsc[index] = value;
    }

    unsigned tableSize() const { return mOsc.size(); }

    float tick() { return mOsc(); }
    void reset() { mOsc.reset(); }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }
    
    float process() override
    {
        return update(mOsc);
    }

private:
    gam::Osc<double> mOsc;
};

class ModPan 
{
public:
    ModPan(float pos = 0.0f)
    : basePos(pos)
    {
        setIM(0.0f);
        setAM(0.0f);
        setFB(0.0f);
        setMix(1.0f);
    }

    // -------- Base controls --------
    void setPos(float p) { basePos = std::clamp(p, -1.0f, 1.0f); }
    void setMix(float m) { baseMix = std::clamp(m, 0.0f, 1.0f); }
    void setFB(float f)  { baseFB  = std::clamp(f, -0.99f, 0.99f); }

    // -------- Modulation depths --------
    void setPM(float v)   { pm.set(v); }
    void setMIXM(float v) { mixm.set(v); }
    void setFBM(float v)  { fbm.set(v); }

    // -------- Gain stages --------
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    // -------- Processing --------
    StereoSample process(float input)
    {
        float _pm   = pm.process();
        float _mixm = mixm.process();
        float _fbm  = fbm.process();

        float pos = basePos + basePos * _pm;
        pos = std::clamp(pos, -1.0f, 1.0f);

        float mix = baseMix + baseMix * _mixm;
        mix = std::clamp(mix, 0.0f, 1.0f);

        float fb = baseFB + baseFB * _fbm;
        fb = std::clamp(fb, -0.99f, 0.99f);

        float in = input * im.process() + last * fb;

        
        pan.setPos(pos);
        StereoSample r = pan.process(in);

        float left  = input * (1.0f - mix) + r.L * mix;
        float right = input * (1.0f - mix) + r.R * mix;
        last = 0.5f * (left + right);
        
        float g = am.process();
        left  *= g;
        right *= g;


        return StereoSample(left,right);
    }

private:
    Pan pan;

    float basePos = 0.0f;
    float baseMix = 1.0f;
    float baseFB  = 0.0f;

    Modulator pm;
    Modulator mixm;
    Modulator fbm;
    Modulator im;
    Modulator am;

    float last = 0.0f;
};

class ModPhasor: public ModGenerator
{
public:

    void setFreq(float v) { baseFreq = v; }
    float process() override
    {
        return update(_phasor);
    }

private:

    AccumPhase _phasor;
};

class ModPM : public Function
{
public:
    ModPM(float maxDelay = 0.05f)
    : buffer(size_t(gam::sampleRate() * maxDelay) + 2),
      maxDelaySamples(float(buffer.size() - 2))
    {
        setDepth(0.0f);
        setFB(0.0f);
        setIM(0.0f);
        setAM(0.0f);
    }

    // -------- Controls --------
    void setDepth(float d) { depth = std::max(d, 0.0f); }
    void setFB(float f)    { fb = std::clamp(f, 0.0f, 0.99f); }

    void setMM(float v) { mm.set(v); }
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float input)
    {
        float m  = mm.process();      // [-1,1]
        float in = input * im.process();

        // Write input with feedback
        buffer[writeIndex] = in + fb * last;

        // Phase-modulated read index
        float phaseOffset = depth * m * maxDelaySamples;
        float readIndex   = writeIndex - phaseOffset;

        // Wrap
        if (readIndex < 0) readIndex += maxDelaySamples;
        if (readIndex >= maxDelaySamples) readIndex -= maxDelaySamples;

        // Linear interpolation
        int i0 = int(readIndex);
        int i1 = (i0 + 1) % buffer.size();
        float frac = readIndex - i0;

        float y = buffer[i0] * (1.0f - frac) + buffer[i1] * frac;

        // Advance
        writeIndex = (writeIndex + 1) % buffer.size();
        last = y;

        return y * am.process();
    }

private:
    std::vector<float> buffer;
    float maxDelaySamples;

    int writeIndex = 0;

    float depth = 0.0f;
    float fb    = 0.0f;

    Modulator mm;
    Modulator im;
    Modulator am;

    float last = 0.0f;
};

class ModQuantizer : public Function
{
public:
    ModQuantizer(float freq = 2000.f, float step = 0.f)
    : mQuant(freq, step),
      baseFreq(freq),
      baseStep(step)
    {
        setIM(0.0f);
        setAM(0.0f);
        setMix(1.0f);
        setFB(0.0f);
    }

    // ---------------- Base controls ----------------
    void setFreq(float f) { baseFreq = std::max(0.01f, f); }
    void setStep(float s) { baseStep = std::max(0.0f, s); }
    void setMix(float m)  { baseMix  = std::clamp(m, 0.0f, 1.0f); }
    void setFB(float f)   { baseFB   = std::clamp(f, -0.99f, 0.99f); }

    // ---------------- Modulation depths ----------------
    void setFM(float v)   { fm.set(v); }
    void setSM(float v)   { sm.set(v); }
    void setMIXM(float v) { mixm.set(v); }
    void setFBM(float v)  { fbm.set(v); }

    // ---------------- Gain stages ----------------
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float input) override
    {
        float _fm   = fm.process();    // [-1,1]
        float _sm   = sm.process();    // [-1,1]
        float _mixm = mixm.process();  // [-1,1]
        float _fbm  = fbm.process();   // [-1,1]

        float freq = baseFreq + baseFreq * _fm;
        float step = baseStep + baseStep * _sm;
        float mix  = baseMix  + baseMix  * _mixm;
        float fb   = baseFB   + baseFB   * _fbm;

        freq = std::max(freq, 0.01f);
        step = std::max(step, 0.0f);
        mix  = std::clamp(mix, 0.0f, 1.0f);
        fb   = std::clamp(fb, -0.99f, 0.99f);

        mQuant.setFreq(freq);
        mQuant.setStep(step);

        float in = input * im.process() + last * fb;
        float wet = mQuant.process(in);

        float out = input * (1.0f - mix) + wet * mix;
        out *= am.process();

        last = wet;
        return out;
    }

private:
    Quantizer mQuant;

    float baseFreq;
    float baseStep;
    float baseMix = 1.0f;
    float baseFB  = 0.0f;

    Modulator fm;
    Modulator sm;
    Modulator mixm;
    Modulator fbm;
    Modulator im;
    Modulator am;

    float last = 0.0f;
};

class ModReson : public ModFilter2
{
public:
    ModReson(float initFreq = 1000.0f, float initWidth = 100.0f)
    {
        setCutoff(initFreq);
        setWidth(initWidth);
        setAM(0.0f);
    }

    float process(float input) override
    {
        return update(mFilter, input);
    }

    void reset() { mFilter.reset(); }

private:
    gam::Reson<double> mFilter;
};

class ModSine : public ModGenerator
{
public:
    ModSine(float f)
    {
        setFreq(f);
        setFM(0);
        setPM(0);
        setAM(1);
        setRatio(1);
        setIndex(1);
    }

    void setFreq(float f)
    {
        baseFreq = f;        // 🔑 always store base frequency
        mOsc.freq(f);
    }

    float process() override
    {
        return update(mOsc);
    }

private:
    gam::Sine<double> mOsc;
};

class ModSweep: public ModGenerator
{
public:

    void setFreq(float v) { baseFreq = v; }
    float process() override
    {
        return update(_phasor);
    }
protected:

    gam::Sweep<> _phasor;
};
