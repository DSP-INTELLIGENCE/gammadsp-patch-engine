#include <cstdint>
#include <cmath>

struct DX7Patch {
    int algorithm = 0;

    float ratios[6] = {};
    float outputLevel[6] = {};

    float egRate[6][4] = {};
    float egLevel[6][4] = {};

    int sensitivity[6] = {};

    uint8_t modMatrix[6][6] = {};
    uint8_t outMatrix[6] = {};

    float pitchEgRate[4];
    float pitchEgLevel[4];

};

struct DX7Envelope {
    float level[4];   // target amplitudes
    float rate[4];    // segment speeds

    int stage = 0;
    float value = 0.f;
    bool active = false;

    void noteOn(){
        stage = 0;
        value = level[3];   // start from release level (DX behavior)
        active = true;
    }



    void noteOff(){
        stage = 3;   // go directly to release stage
    }

    float process(){
        if(!active) return 0.f;

        float target = level[stage];
        float speed  = rate[stage];

        value += (target - value) * speed;

        // advance stage when close
        if (fabs(value - target) < 1e-4f && stage < 3)
            stage++;

        if(stage == 3 && value < 1e-5f)
            active = false;

        return value;
    }
};

struct DX7PitchEnvelope {
    DX7Envelope env;

    void setup(const DX7Patch& p){
        for(int i=0;i<4;i++){
            env.level[i] = (p.pitchEgLevel[i] - 50) / 50.0f; // center at 0
            env.rate[i]  = (p.pitchEgRate[i] / 99.0f) * 0.15f;
        }
    }

    void noteOn(){ env.noteOn(); }
    void noteOff(){ env.noteOff(); }

    float process(){
        return env.process();   // returns pitch offset in octaves
    }
};

// DX7
class DX7Oscillator : public Generator {
public:
    DX7Oscillator(float f = 440.f);

    void setFreq(float f);
    void setRatio(double r);
    void setDetune(double d);
    void setPhase(double p);            // normalized [-1..1]
    void setFeedback(double f);         // 0..1
    void setPMDepth(double d);          // modulation index
    void setMod(double v);
    void setAM(double v);
    double getFeedback() { return feedback.process(); } // or .get() if your Modulator supports it

    float process() override;     // modInput is sum of modulators

    Modulator mod;
    Modulator am;
    Modulator ratio;
    Modulator detune;
    Modulator pmdepth;
    Modulator feedback;

private:
    double freq = 440.0;    
    double phase = -1.0;                // [-1..1]
    double lastOut = 0.0;
};


struct DX7Operator {
    DX7Oscillator osc;
    DX7Envelope env;

    float level = 1.0f;
    float keyScale = 1.0f;

    float lastOut = 0.f;   // for feedback

    void noteOn(){
        lastOut = 0.f;
        env.noteOn();
    }

    void noteOff(){
        env.noteOff();
    }

    float process(float mod){
        // DX feedback: self-modulation at phase input
        osc.setMod(mod + lastOut * osc.getFeedback());

        float y = osc.process();

        // Apply DX envelope + key scaling + output level
        float out = y * (env.process() * keyScale) * level;

        lastOut = out;
        return out;
    }
};

struct FMVoice6 {
    DX7Operator op[6];
    DX7Patch patch;
    DX7PitchEnvelope pitchEnv;

    float baseFreq = 440.0f;

    void loadPatch(const DX7Patch& p)
    {
        patch = p;

        for(int i = 0; i < 6; ++i){
            op[i].osc.setRatio(patch.ratios[i]);
            op[i].level = patch.outputLevel[i];
            setupEnvelope(op[i].env, patch, i);
        }

        pitchEnv.setup(patch);
    }

    void noteOn(int midiNote, float freq)
    {
        baseFreq = freq;

        float dist = (midiNote - 60) / 36.0f;

        for(int i=0;i<6;i++){
            float sens = patch.sensitivity[i] / 7.0f;
            float k = 1.0f + dist * sens;

            if(k < 0.1f) k = 0.1f;
            if(k > 4.0f) k = 4.0f;

            op[i].keyScale = k;
        }

        for(auto& o : op) o.noteOn();
        pitchEnv.noteOn();
    }
    void noteOff(){
        for(auto& o : op) o.noteOff();
        pitchEnv.noteOff();
    }

    float process(){
        float out[6] = {0};

        // Advance pitch envelope ONCE per sample
        float pitchOffset = pitchEnv.process();     // in octaves
        float freq = baseFreq * powf(2.0f, pitchOffset);

        // DX operator evaluation: OP6 → OP1
        for(int i = 5; i >= 0; --i){
            float modSum = 0;

            for(int m = 0; m < 6; ++m){
                if(patch.modMatrix[m][i])
                    modSum += out[m];
            }

            op[i].osc.setFreq(freq);
            out[i] = op[i].process(modSum);
        }

        // Sum carriers
        float mix = 0;
        for(int i = 0; i < 6; ++i){
            if(patch.outMatrix[i])
                mix += out[i];
        }

        return mix * 0.25f;
    }

};



#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

void setupEnvelope(DX7Envelope& env, const DX7Patch& p, int op)
{
    for(int i = 0; i < 4; ++i)
    {
        // DX7 levels are logarithmic loudness values
        float L = p.egLevel[op][i] / 99.0f;

        // DX7 rates are speeds, not times
        float R = p.egRate[op][i] / 99.0f;

        // Convert to usable exponential envelope parameters
        env.level[i] = L;

        // Quadratic curve gives DX-like timing response
        env.rate[i] = 0.001f + (R * R) * 0.25f;
    }

    // Initialize envelope state for clean retrigger
    env.stage = 3;
    env.value = env.level[3];
    env.active = false;
}

bool loadPatch(const std::string& file, DX7Patch& p)
{
    std::ifstream f(file);
    if(!f.is_open())
        return false;

    nlohmann::json j;
    try {
        f >> j;
    }
    catch (...) {
        return false;
    }

    p.algorithm = j.value("algorithm", 0);

    // --- Operator parameters ---
    for(int i = 0; i < 6; ++i)
    {
        p.ratios[i]      = j["ratios"][i].get<float>();
        p.outputLevel[i]= j["outputLevel"][i].get<float>();
        p.sensitivity[i]= j["sensitivity"][i].get<int>();

        for(int k = 0; k < 4; ++k)
        {
            p.egRate[i][k]  = j["egRate"][k][i].get<float>();
            p.egLevel[i][k] = j["egLevel"][k][i].get<float>();
        }
    }

    // --- Modulation matrix (mod -> target) ---
    for(int m = 0; m < 6; ++m)
        for(int t = 0; t < 6; ++t)
            p.modMatrix[m][t] = (uint8_t) j["modMatrix"][m][t].get<int>();

    // --- Carrier output mask ---
    for(int i = 0; i < 6; ++i)
        p.outMatrix[i] = (uint8_t) j["outMatrix"][i].get<int>();

    // --- Pitch envelope ---
    for(int i = 0; i < 4; ++i)
    {
        p.pitchEgRate[i]  = j["pitchEgRate"][i].get<float>();
        p.pitchEgLevel[i] = j["pitchEgLevel"][i].get<float>();
    }

    return true;
}
