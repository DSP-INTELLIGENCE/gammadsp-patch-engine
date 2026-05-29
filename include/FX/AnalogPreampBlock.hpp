#pragma once
#include <cmath>
#include <algorithm>
#include "TPT1Pole.hpp"

// Assumes your codebase provides:
//  - TPT1Pole  (from your earlier post: setLPCut/setHPCut/processLP/processHP, members g,s,a)
//  - Modulator
//  - Function

// ---------------------------
// Triode device (Koren-ish, simplified)
// ---------------------------
struct Triode
{
    // 12AX7-ish defaults
    float mu  = 100.f;
    float kg1 = 1060.f;
    float kp  = 600.f;
    float ex  = 1.4f;
    float vct = 0.0f;

    inline float process(float vgk, float vpk)
    {
        // Effective grid-plate voltage
        float e = vgk + vct + vpk / std::max(1e-6f, mu);

        // Cutoff (no negative plate current)
        e = std::max(e, 0.f);

        // Soft saturation in "drive domain"
        e = std::tanh(0.7f * e);

        // Plate resistance interaction (keep denom sane)
        float denom = 1.f + kp * std::max(0.05f, vpk);

        float ip = kg1 * std::pow(e, ex) / denom;

        // Physical clamp: never negative
        return std::clamp(ip, 0.f, 10.f);
    }
};

// ---------------------------
// Triode stage (your style)
// ---------------------------
struct TriodeStage
{
    float sr = 44100.f;
    float plateGain = 1.2f;   // start here

    Triode triode;

    TPT1Pole inputHP;     // coupling cap (HP)
    TPT1Pole biasLP;      // bias memory (LP)
    TPT1Pole plateLP;     // plate RC load (LP integrator-ish)

    float bias = 0.f;

    float drive      = 2.5f;   // input gain
    float biasAmount = 0.4f;   // env -> bias depth

    void init(float fs)
    {
        sr = std::max(1.f, fs);

        inputHP.setHPCut(20.f, sr);
        biasLP.setLPCut(5.f, sr);
        plateLP.setLPCut(3000.f, sr);

        reset();
    }

    void reset()
    {
        inputHP.reset(0.f);
        biasLP.reset(0.f);
        plateLP.reset(1.f); // give plate some non-zero starting point
        bias = 0.f;
    }

    inline float process(float x)
    {
        // Coupling cap / DC block
        x = inputHP.processHP(x);

        // Envelope -> slow bias migration
        float env = std::fabs(x);
        float targetBias = -biasAmount * env;
        bias = biasLP.processLP(targetBias);

        // Grid-cathode voltage
        float vgk = x * drive + bias;

        // Grid conduction softening (positive grid region)
        // (simple but effective)
        if (vgk > 0.f) vgk *= 0.5f;

        // Plate voltage from previous integrator state
        float vpk = std::max(0.1f, plateLP.s);

        // Plate current
        float ip = triode.process(vgk, vpk);

        // Integrate current → plate voltage
        float vplate = plateLP.processLP(-ip);

        // Realistic voltage swing
        float y = vplate * plateGain;

        // Clamp plate state
        plateLP.s = std::max(0.1f, plateLP.s);

        return y;

    }
};

// ---------------------------
// Nonlinear ZDF Ladder (cheap 1-step Newton)
// 4x TPT1Pole cascade + tanh feedback nonlinearity
// ---------------------------
struct NonlinearZDFLadder
{
    float sr = 44100.f;

    TPT1Pole p1, p2, p3, p4;

    float cutoffHz  = 1000.f;
    float resonance = 0.0f;   // 0..1.2 (ish)
    float drive     = 1.0f;   // input drive

    float k     = 0.0f;       // feedback amount
    float alpha = 0.0f;       // per-pole gain g/(1+g)

    static inline float sat(float x)   { return std::tanh(x); }
    static inline float sat_d(float x) { float t = std::tanh(x); return 1.f - t*t; }

    void init(float fs)
    {
        sr = std::max(1.f, fs);
        reset();
        set(cutoffHz, resonance);
    }

    void reset()
    {
        p1.reset(0.f); p2.reset(0.f); p3.reset(0.f); p4.reset(0.f);
    }

    void set(float fc, float res01)
    {
        cutoffHz  = std::clamp(fc, 5.f, 0.49f * sr);
        resonance = std::clamp(res01, 0.f, 1.2f);

        // classic ladder scaling
        k = 4.f * resonance;

        p1.setLPCut(cutoffHz, sr);
        p2.setLPCut(cutoffHz, sr);
        p3.setLPCut(cutoffHz, sr);
        p4.setLPCut(cutoffHz, sr);

        // TPT 1-pole small-signal alpha
        alpha = p1.g / (1.f + p1.g);
    }

    inline float evalCascade(float u) const
    {
        // copy states (cheap, 4 structs)
        TPT1Pole t1 = p1, t2 = p2, t3 = p3, t4 = p4;
        float y1 = t1.processLP(u);
        float y2 = t2.processLP(y1);
        float y3 = t3.processLP(y2);
        float y4 = t4.processLP(y3);
        return y4;
    }

    inline float runCascade(float u)
    {
        float y1 = p1.processLP(u);
        float y2 = p2.processLP(y1);
        float y3 = p3.processLP(y2);
        float y4 = p4.processLP(y3);
        return y4;
    }

    inline float process(float x)
    {
        x *= drive;

        // Solve: u + k*sat(y4(u)) - x = 0
        // start from previous output state proxy
        float u0 = x - k * sat(p4.s);

        float y4_0 = evalCascade(u0);

        // dy4/du approx ~ alpha^4
        float dydu = alpha;
        dydu *= dydu; // a^2
        dydu *= dydu; // a^4

        float F  = u0 + k * sat(y4_0) - x;
        float dF = 1.f + k * sat_d(y4_0) * dydu;

        float u1 = u0;
        if (std::fabs(dF) > 1e-6f)
            u1 = u0 - F / dF;

        // optional input nonlinearity (ladder vibe)
        u1 = sat(u1);

        return runCascade(u1);
    }
};

// ---------------------------
// AnalogPreampBlock
// ---------------------------
class AnalogPreampBlock : public Function
{
public:
    enum TubeType { TUBE_12AX7=0, TUBE_12AT7=1, TUBE_12AU7=2 };

    // public knobs (0..1)
    float baseGain01  = 0.35f;
    float baseTone01  = 0.55f;
    float baseRes01   = 0.15f;
    float baseLevel01 = 0.70f;

    
    // modulators
    Modulator gm, tm, rm, lm;

    AnalogPreampBlock() = default;

    void init(float sampleRate)
    {
        sr = std::max(1.f, sampleRate);

        // conditioning
        preHP.setHPCut(20.f, sr);
        preLP.setLPCut(12000.f, sr);

        postDC.setHPCut(10.f, sr);
        postLP.setLPCut(8000.f, sr);
        postHP.setHPCut(20.f, sr);

        // core
        tube.init(sr);
        ladder.init(sr);
        ladder.drive = 1.8f;  // start here

        setTubeType(TUBE_12AX7);

        reset();
    }

    void reset()
    {
        preHP.reset(0.f);
        preLP.reset(0.f);

        postDC.reset(0.f);
        postLP.reset(0.f);
        postHP.reset(0.f);

        tube.reset();
        ladder.reset();

        lastOut = 0.f;
    }

    // knob setters (0..1)
    void setGain(float v01)  { baseGain01  = std::clamp(v01, 0.f, 1.f); }
    void setTone(float v01)  { baseTone01  = std::clamp(v01, 0.f, 1.f); }
    void setRes (float v01)  { baseRes01   = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) { baseLevel01 = std::clamp(v01, 0.f, 1.f); }

    void setTubeType(int type)
    {
        tubeType = (TubeType)std::clamp(type, (int)TUBE_12AX7, (int)TUBE_12AU7);

        switch (tubeType)
        {
            default:
            case TUBE_12AX7:
                tube.triode.mu  = 100.f;
                tube.triode.kg1 = 1060.f;
                tube.triode.kp  = 600.f;
                tube.triode.ex  = 1.40f;
                tube.triode.vct = 0.00f;
                tubeBaseDriveMul = 1.20f;
                tube.biasAmount  = 0.42f;
                break;

            case TUBE_12AT7:
                tube.triode.mu  = 60.f;
                tube.triode.kg1 = 1400.f;
                tube.triode.kp  = 520.f;
                tube.triode.ex  = 1.33f;
                tube.triode.vct = 0.00f;
                tubeBaseDriveMul = 1.00f;
                tube.biasAmount  = 0.34f;
                break;

            case TUBE_12AU7:
                tube.triode.mu  = 20.f;
                tube.triode.kg1 = 1800.f;
                tube.triode.kp  = 450.f;
                tube.triode.ex  = 1.25f;
                tube.triode.vct = 0.00f;
                tubeBaseDriveMul = 0.80f;
                tube.biasAmount  = 0.28f;
                break;
        }
    }

    float process(float x) override
    {
        // Modulated knobs (your "base + base*mod" convention)
        float g01 = std::clamp(baseGain01  + baseGain01  * gm.process(), 0.f, 1.f);
        float t01 = std::clamp(baseTone01  + baseTone01  * tm.process(), 0.f, 1.f);
        float r01 = std::clamp(baseRes01   + baseRes01   * rm.process(), 0.f, 1.f);
        float l01 = std::clamp(baseLevel01 + baseLevel01 * lm.process(), 0.f, 1.f);

        // ---- mappings ----
        // Gain -> 0..55 dB (square gives better low-end resolution)
        float gSh = g01 * g01;
        float driveDb  = 55.f * gSh;
        float driveLin = std::pow(10.f, driveDb / 20.f) * tubeBaseDriveMul;

        // Tone -> ladder cutoff (log)
        float fc = mapLog(t01, 150.f, 8000.f);

        // Res -> ladder resonance
        float res = 1.10f * (r01 * r01);

        // Level -> -18..+18 dB
        float levelDb = -18.f + 36.f * l01;
        float outGain = std::pow(10.f, levelDb / 20.f);

        tube.drive = driveLin;
        ladder.set(fc, res);

        // ---- signal path ----
        x = preHP.processHP(x);
        x = preLP.processLP(x);

        x = tube.process(x);

        x = postDC.processHP(x);

        x = ladder.process(x);

        // track tone a bit for post smoothing
        postLP.setLPCut(mapLog(t01, 2500.f, 12000.f), sr);
        x = postLP.processLP(x);
        x = postHP.processHP(x);

        float y = x * outGain;
        lastOut = y;
        return y;
    }

    void run(const float* in, float* out, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }

private:
    float sr = 44100.f;

    // conditioning
    TPT1Pole preHP, preLP;
    TPT1Pole postDC, postLP, postHP;

    // core
    TriodeStage tube;
    NonlinearZDFLadder ladder;

    // tube selection
    TubeType tubeType = TUBE_12AX7;
    float tubeBaseDriveMul = 1.0f;
    
    float lastOut = 0.f;

    static inline float mapLog(float norm01, float lo, float hi)
    {
        norm01 = std::clamp(norm01, 0.f, 1.f);
        lo = std::max(lo, 1e-6f);
        hi = std::max(hi, lo * 1.0001f);
        float r = hi / lo;
        return lo * std::pow(r, norm01);
    }
};
