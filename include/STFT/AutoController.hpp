#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

enum CurveType { CURVE_LINEAR, CURVE_EXP, CURVE_LOG, CURVE_SMOOTH };
enum ConditionType { COND_ALWAYS, COND_WHEN_ACTIVE, COND_WHEN_ONSET, COND_WHEN_BEAT, COND_WHEN_ENERGY_HIGH };

struct ModulationSource {
    float* value;
    float  weight;
};

struct LFO {
    float phase = 0.0f;
    float rate  = 0.0f;   // Hz
    float depth = 0.0f;   // 0..1
};

struct ModEnvelope {
    float value = 0.0f;
    float attack = 0.01f;
    float decay  = 0.1f;
    float sustain = 0.0f;
    bool  active = false;

    float velAmount = 1.0f;
    float velScale  = 1.0f;
};

struct ModulationLink {
    ModulationSource sources[4];
    int sourceCount = 0;

    float* target = nullptr;

    float minOut = 0.0f;
    float maxOut = 1.0f;
    float smoothing = 0.05f;
    float value = 0.0f;

    CurveType curve = CURVE_LINEAR;
    ConditionType condition = COND_ALWAYS;

    bool*  active = nullptr;
    bool*  onset  = nullptr;
    bool*  beat   = nullptr;
    float* energy = nullptr;

    LFO lfo;
    ModEnvelope env;

    bool* envTriggerOnset = nullptr;
    bool* envTriggerBeat  = nullptr;

    float* velocitySource = nullptr;
};

class AutoController {
public:
    AutoController(float sampleRate);

    void reset();

    int beginLink(float* target,
                  float minOut, float maxOut,
                  float smoothing,
                  CurveType curve,
                  ConditionType cond,
                  bool* active = nullptr,
                  bool* onset  = nullptr,
                  bool* beat   = nullptr,
                  float* energy = nullptr);

    void addSource(int id, float* source, float weight);
    void setLFO(int id, float rateHz, float depth);
    void setEnvelope(int id, float attack, float decay, float sustain,
                     bool* trigOnset, bool* trigBeat);
    void setVelocity(int id, float* velocitySource, float amount);

    void process();

private:
    float mSampleRate;
    std::vector<ModulationLink> mLinks;

    float applyCurve(float x, CurveType c);
};
