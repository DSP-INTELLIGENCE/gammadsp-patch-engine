// PolyDX7.hpp
#pragma once
#include <vector>
#include <memory>
#include <array>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdint>

#include "PolyBLEP.hpp"


class EnvelopeGenerator {
public:
    EnvelopeGenerator() = default;

    void configure(const int rates[4], const int levels[4], int outLevel);
    int32_t tick();
    void noteOn()  { stage = 0; down = false; advanceStage(); }
    void noteOff() { stage = 3; down = true;  advanceStage(); }

    int32_t getLevel() const { return level; }

private:
    int r[4]{}, l[4]{};
    int outLevel = 0;

    int32_t level = 0, target = 0, inc = 0;
    int stage = 0;
    bool rising = false, down = false;

    void reset(){ level = 0; stage = 0; advanceStage(); }
    void advanceStage();    
};

class EGParam : public Param {
public:
    EnvelopeGenerator* eg = nullptr;
    double scale = 1.0;

    EGParam() = default;
    EGParam(EnvelopeGenerator* e, double s) : eg(e), scale(s) {}

    void set(float) override {}

    float next() override {
        if(!eg) return 0.f;
        double q = eg->getLevel();
        return (float)std::exp2(q * (1.0 / (1 << 24)) - 14.0) * (float)scale;
    }
};


class Operator {
public:
    PolyBLEP osc;
    EnvelopeGenerator eg;
    EGParam ampParam, fmParam;

    double baseFreq = 440.0;
    double ratio = 1.0;
    // in Operator class (header)
    float last = 0.f;


    Operator(double sr);
    
    void noteOn()  { eg.noteOn(); }
    void noteOff() { eg.noteOff(); }
    float tick();
    void setBaseFreq(double f){ baseFreq = f; }
};

struct Connection {
    int src;
    int dst;
    double depth;
};

struct Edge { 
    uint8_t src, dst; 
    float   depth;
};

struct AlgorithmDef {
    std::vector<Edge> modEdges;
    std::vector<uint8_t> carriers;
};

class Algorithm {
public:
    std::array<Operator*,6> ops;
    const AlgorithmDef* def = nullptr;

    void applyRouting();
    float tick();    
};

struct Patch
{
    std::string name;

    int algorithm = 0;

    float ratio[6]       = {1,1,1,1,1,1};
    int   outLevel[6]    = {0,0,0,0,0,0};
    int   velSens[6]     = {0,0,0,0,0,0};

    int egRate[4][6]  = {};
    int egLevel[4][6] = {};

    bool fixedFreq[6] = {false,false,false,false,false,false};

    int transpose = 0;
};


class DX7SysexLoader
{
public:
    bool loadFromFile(const std::string& path);
    Patch loadPatch(int index = 0);
    int scaleOutLevel(int ol);
    int scaleVelocity(int velocity, int sensitivity);

private:
    std::vector<uint8_t> raw;

    void unpackPatch(const uint8_t* src, uint8_t* dst);
    float computeFreq(int coarse, int fine, int detune);

};


class Voice
{
public:
    
    Voice(double sampleRate);
    void applyPatch(const Patch& p, int velocity);
    void noteOn(int midiNote, int velocity);
    void noteOff();
    float tick();
    void initOps(double sr);

private:
    std::unique_ptr<Operator> ops[6];
    Algorithm algo;

    Patch currentPatch;
    DX7SysexLoader sysex;

    std::vector<uint8_t> carriers;

    double baseFreq = 440.0;
    bool active = false;
};