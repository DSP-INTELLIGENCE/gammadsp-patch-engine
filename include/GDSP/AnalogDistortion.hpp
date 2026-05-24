#pragma once
#include <cmath>
#include <algorithm>
#include "TPT1Pole.hpp"
#include "Waveshaper.hpp"

struct InputStage {
    TPT1Pole hp;     // coupling cap
    TPT1Pole lp;     // bandwidth limit
    TPT1Pole envLP;  // envelope follower

    float process(float x) {
        x = hp.processHP(x);
        x = lp.processLP(x);
        return x;
    }

    float envelope(float x) {
        return envLP.processLP(std::fabs(x));
    }

    void reset() {
        hp.reset(); lp.reset(); envLP.reset();
    }
};

struct NonLinearStage {
    Waveshaper* shaper = nullptr;

    TPT1Pole biasLP;   // bias memory
    TPT1Pole sagLP;    // supply memory

    float baseBias   = 0.f;
    float biasAmount = 0.f;
    float sagAmount  = 0.f;

    float bias = 0.f;

    float process(float x, float env, float drive) {
        if(!shaper) return x; // safety

        // bias migration
        float targetBias = baseBias - biasAmount * env;
        bias = biasLP.processLP(targetBias);

        // nonlinear device
        float y = shaper->process(x * drive + bias);

        // sag compression with memory
        float sagEnv = sagLP.processLP(env);
        float sag = 1.f / (1.f + sagAmount * sagEnv);
        return y * sag;
    }

    void reset() {
        biasLP.reset();
        sagLP.reset();
        bias = 0.f;
    }
};

struct OutputStage {
    TPT1Pole lp;
    TPT1Pole hp;
    Biquad   mid;   // peaking EQ

    float process(float x, float tone01) {
        float dark   = lp.processLP(x);
        float bright = hp.processHP(x);
        float y = (1.f - tone01) * dark + tone01 * bright;

        // Actually apply the mid EQ (critical for “pedal identity”)
        y = mid.process(y); // adapt if your Biquad uses operator()

        return y;
    }

    void reset() {
        lp.reset(); hp.reset(); mid.reset();
    }
};


class AnalogDistortion
{
public:
    // ===== stages =====
    InputStage     in;
    NonLinearStage core;
    OutputStage    out;
    Waveshaper      shaper;

    // ===== user knobs =====
    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;

    // ===== mappings =====
    float driveMaxDb  = 40.f;
    float levelMinDb  = -18.f;
    float levelMaxDb  = 12.f;
    bool  toneReverse = false;

    float inTrim  = 1.f;  // before in stage
    float outTrim = 1.f;  // after everything


    float process(float x)
    {
        float xin = in.process(x * inTrim);
        float env = in.envelope(xin);

        float d = drive01 * drive01;
        float driveDb = driveMaxDb * d;
        float drive = std::pow(10.f, driveDb / 20.f);

        float y = core.process(xin, env, drive);

        float t = toneReverse ? (1.f - tone01) : tone01;
        y = out.process(y, t);

        float levelDb = levelMinDb + (levelMaxDb - levelMinDb) * level01;
        float gain = std::pow(10.f, levelDb / 20.f);
        return y * gain * outTrim;        
    }

    void reset()
    {
        in.reset();
        core.reset();
        out.reset();
    }
};

class TubeScreamer : public AnalogDistortion
{
public:
    TubeScreamer(float sr)
    {
        // ---------- input ----------
        in.hp.setCut(100.f, sr);
        in.lp.setCut(4500.f, sr);
        in.envLP.setCut(18.f, sr);

        // ---------- core ----------
        core.shaper     = &shaper;
        core.baseBias   = 0.f;
        core.biasAmount = 0.15f;
        core.sagAmount  = 0.08f;

        core.biasLP.setCut(6.f, sr);
        core.sagLP .setCut(4.f, sr);

        shaper.setMode(Waveshaper::WS_SOFTEXP);

        // ---------- output ----------
        out.lp.setCut(1800.f, sr);
        out.hp.setCut(720.f, sr);
        out.mid.setType(gam::PEAKING);
        out.mid.setFreq(720.f);
        out.mid.setRes(0.8f);
        out.mid.setLevel(+3.5f);
        

        // ---------- mapping ----------
        driveMaxDb = 40.f;
        levelMinDb = -18.f;
        levelMaxDb = +12.f;
    }
};

class RAT : public AnalogDistortion
{
public:
    RAT(float sr)
    {
        in.hp.setCut(30.f, sr);
        in.lp.setCut(9000.f, sr);
        in.envLP.setCut(14.f, sr);

        core.shaper     = &shaper;
        core.baseBias   = 0.f;
        core.biasAmount = 0.20f;
        core.sagAmount  = 0.15f;

        core.biasLP.setCut(5.f, sr);
        core.sagLP .setCut(3.5f, sr);

        shaper.setMode(Waveshaper::WS_ARCCLIP);

        out.lp.setCut(6000.f, sr);
        out.hp.setCut(600.f, sr);
        out.mid.setType(gam::PEAKING);
        out.mid.setFreq(1600.0f);
        out.mid.setRes(0.9f);
        out.mid.setLevel(+2.0f);
        

        driveMaxDb  = 55.f;
        levelMinDb  = -18.f;
        levelMaxDb  = +12.f;
        toneReverse = true;   // RAT filter works backwards
    }
};

class Klon : public AnalogDistortion
{
public:
    float cleanMix = 0.65f;

    Klon(float sr)
    {
        in.hp.setCut(90.f, sr);
        in.lp.setCut(7000.f, sr);
        in.envLP.setCut(16.f, sr);

        core.shaper     = &shaper;
        core.baseBias   = 0.f;
        core.biasAmount = 0.10f;
        core.sagAmount  = 0.05f;

        core.biasLP.setCut(6.f, sr);
        core.sagLP .setCut(5.f, sr);

        shaper.setMode(Waveshaper::WS_SOFTEXP);

        out.lp.setCut(9000.f, sr);
        out.hp.setCut(900.f, sr);
        out.mid.setType(gam::PEAKING);
        out.mid.setFreq(1000.0f);
        out.mid.setRes(0.7f);
        out.mid.setLevel(+1.5f);
        

        driveMaxDb = 35.f;
    }

    float process(float x)
    {
        float dirty = AnalogDistortion::process(x);
        float clean = x * 1.05f;
        return cleanMix * clean + (1.f - cleanMix) * dirty;
    }
};

class BigMuff : public AnalogDistortion
{
public:
    BigMuff(float sr)
    {
        in.hp.setCut(20.f, sr);
        in.lp.setCut(5000.f, sr);
        in.envLP.setCut(20.f, sr);

        core.shaper     = &shaper;
        core.baseBias   = 0.05f;
        core.biasAmount = 0.08f;
        core.sagAmount  = 0.20f;

        core.biasLP.setCut(4.f, sr);
        core.sagLP .setCut(2.5f, sr);

        shaper.setMode(Waveshaper::WS_HARDCLIP);

        out.lp.setCut(8000.f, sr);
        out.hp.setCut(350.f, sr);
        out.mid.setType(gam::PEAKING);
        out.mid.setFreq(1000.0f);
        out.mid.setRes(0.7f);
        out.mid.setLevel(-6.0f);
        
        driveMaxDb = 60.f;
        levelMinDb = -24.f;
        levelMaxDb = +6.f;
    }
};



