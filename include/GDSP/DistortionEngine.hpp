#pragma once
#include <memory>
#include <algorithm>
#include <cmath>
#include "TPT1Pole.hpp"
#include "Waveshaper.hpp"

// Uses your TPT1Pole
struct AnalogStage
{
    float sr = 44100.f;

    // DSP blocks
    TPT1Pole couplingHP;   // blocking / DC removal
    TPT1Pole storageLP;    // bias memory / sag
    TPT1Pole envLP;        // envelope follower

    Waveshaper* shaper = nullptr;

    // parameters
    float drive      = 1.0f;
    float baseBias   = 0.0f;
    float biasAmount = 0.35f;  // how much envelope moves bias
    float sagAmount  = 0.25f;

    // state
    float bias = 0.f;

    // ---------- setup ----------
    void init(float sampleRate, Waveshaper *ws){
        sr = sampleRate;
        shaper = ws;

        couplingHP.setCut(10.f, sr);   // typical coupling cap
        storageLP.setCut(5.f, sr);     // bias memory
        envLP.setCut(15.f, sr);        // envelope smoothing
    }

    void reset(){
        couplingHP.reset();
        storageLP.reset();
        envLP.reset();
        bias = 0.f;
    }

    // ---------- main process ----------
    float process(float x)
    {
        // 1) DC blocking / coupling cap
        x = couplingHP.processHP(x);

        // 2) Envelope of *input* (what the device actually sees)
        float env = envLP.processLP(std::fabs(x));

        // 3) Bias memory (slow control system)
        float targetBias = baseBias - biasAmount * env;
        bias = storageLP.processLP(targetBias);

        // 4) Drive + bias into device
        float v = x * drive + bias;

        // 5) Safety clamp (not tone shaping)
        //v = std::clamp(v, -8.f, 8.f);

        // 6) Nonlinear device law
        float y = shaper->process(v);

        // 7) Power-supply sag / compression
        float sag = 1.f / (1.f + sagAmount * env);

        return y * sag;
    }
};

class DistortionCircuit {
public:
    virtual void prepare(float sampleRate) = 0;
    virtual float process(float x) = 0;
    virtual void reset() = 0;

    virtual void setDrive(float v01) = 0;
    virtual void setTone(float v01) = 0;
    virtual void setLevel(float v01) = 0;

    virtual std::string name() const = 0;
};

class BigMuffCircuit : public DistortionCircuit {
public:
    void prepare(float sampleRate) override {
        sr = sampleRate;

        // ===== Stage 1: Input fuzz =====
        shaper1.setMode(Waveshaper::WS_CUBASIC);
        stage1.init(sr, &shaper1);

        stage1.couplingHP.setCut(10.f, sr);
        stage1.envLP.setCut(10.f, sr);
        stage1.storageLP.setCut(4.f, sr);

        stage1.baseBias   = 0.f;
        stage1.biasAmount = 0.30f;
        stage1.sagAmount  = 0.20f;

        // ===== Stage 2: Sustain fuzz =====
        shaper2.setMode(Waveshaper::WS_HARDCLIP);
        stage2.init(sr, &shaper2);

        stage2.couplingHP.setCut(10.f, sr);
        stage2.envLP.setCut(8.f, sr);
        stage2.storageLP.setCut(3.f, sr);

        stage2.baseBias   = 0.f;
        stage2.biasAmount = 0.25f;
        stage2.sagAmount  = 0.15f;

        // ===== Stage 3: Tone stack shaping =====
        toneLP.setCut(4000.f, sr);
        toneHP.setCut(800.f, sr);

        setDrive(0.5f);
        setTone(0.5f);
        setLevel(0.5f);
    }

    void reset() override {
        stage1.reset();
        stage2.reset();
        toneLP.reset();
        toneHP.reset();
    }

    float process(float x) override {
        // Drive: controls both fuzz stages
        float dB = 60.f * drive01 * drive01;
        float g = std::pow(10.f, dB / 20.f);

        stage1.drive = g;
        stage2.drive = g * 0.7f;

        float y = stage1.process(x);
        y = stage2.process(y);

        // Big Muff tone stack (mid scoop blend)
        float low  = toneLP.processLP(y);
        float high = toneHP.processHP(y);
        y = (1.f - tone01) * low + tone01 * high;

        // Level: -24 → +6 dB
        float levelDB = -24.f + 30.f * level01;
        float gain = std::pow(10.f, levelDB / 20.f);

        return y * gain;
    }

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "Big Muff Pi"; }

private:
    float sr = 44100.f;

    // Two fuzz stages
    AnalogStage stage1;
    AnalogStage stage2;

    Waveshaper shaper1;
    Waveshaper shaper2;

    // Tone stack
    TPT1Pole toneLP;
    TPT1Pole toneHP;

    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;
};



// assumes TPT1Pole + AnalogStage + Waveshaper exist
struct DiodeClipperCircuit : public DistortionCircuit
{
    float sr = 44100.f;

    Waveshaper  shaper;
    AnalogStage stage;

    TPT1Pole preLP;
    TPT1Pole postLP;

    // user parameters
    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;

    // smoothed controls
    float driveLP = 0.5f;
    float toneLP  = 0.5f;
    float levelLP = 0.5f;

    // DC blocker
    float dc_x1 = 0.f, dc_y1 = 0.f;

    // -------------------------------------------------

    void prepare(float sampleRate) override
    {
        sr = sampleRate;
        reset();

        stage.init(sr, &shaper);

        stage.couplingHP.setCut(40.f, sr);
        stage.envLP.setCut(20.f, sr);
        stage.storageLP.setCut(3.f, sr);

        stage.baseBias   = 0.f;
        stage.biasAmount = 0.03f;
        stage.sagAmount  = 0.02f;

        preLP.setCut(7000.f, sr);
        postLP.setCut(6000.f, sr);

        shaper.setMode(Waveshaper::WS_ARCCLIP);
        shaper.setDrive(1.f);
        shaper.setCurve(1.f);
        shaper.setOutGain(1.f);
        shaper.setAM(1.f);
    }

    void reset() override
    {
        stage.reset();
        preLP.reset();
        postLP.reset();

        driveLP = drive01;
        toneLP  = tone01;
        levelLP = level01;

        dc_x1 = dc_y1 = 0.f;
    }

    // -------------------------------------------------

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "Diode Clipper"; }

    // -------------------------------------------------

    inline float zap(float x) const
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    inline float dcBlock(float x)
    {
        float y = x - dc_x1 + 0.995f * dc_y1;
        dc_x1 = x; dc_y1 = y;
        return y;
    }

    // -------------------------------------------------

    float process(float x) override
    {
        // smooth controls
        driveLP += 0.002f * (drive01 - driveLP);
        toneLP  += 0.002f * (tone01  - toneLP);
        levelLP += 0.002f * (level01 - levelLP);

        // --- control mapping ---

        // Drive: 0..+50 dB
        float dB = 50.f * (driveLP * driveLP);
        stage.drive = std::pow(10.f, dB / 20.f);

        // Tone control
        float postHz = 1800.f * std::pow(10.f, std::log10(9000.f / 1800.f) * toneLP);
        postLP.setCut(postHz, sr);

        // Output level: -24..+6 dB
        float leveldB = -24.f + 30.f * levelLP;
        float outGain = std::pow(10.f, leveldB / 20.f);

        // -------------------------------------------------
        // Circuit flow
        // -------------------------------------------------

        x = stage.couplingHP.processHP(x);
        x = preLP.processLP(x);

        float env = stage.envLP.processLP(std::fabs(x));

        float targetBias = stage.baseBias - stage.biasAmount * env;
        stage.bias = stage.storageLP.processLP(targetBias);
        stage.bias = zap(stage.bias);

        float y = stage.shaper->process(x * stage.drive + stage.bias);

        float sag = 1.f / (1.f + stage.sagAmount * env);
        y *= sag;

        y = postLP.processLP(y);

        return dcBlock(zap(y * outGain));
    }
};

class DS1Circuit : public DistortionCircuit {
public:
    void prepare(float sampleRate) override {
        sr = sampleRate;

        // Device model: harder, more aggressive clipping
        shaper.setMode(Waveshaper::WS_ARCCLIP);

        // Core analog stage
        stage.init(sr, &shaper);

        // DS-1 style defaults
        stage.couplingHP.setCut(70.f, sr);
        stage.envLP.setCut(12.f, sr);
        stage.storageLP.setCut(3.f, sr);

        stage.baseBias   = 0.f;
        stage.biasAmount = 0.05f;
        stage.sagAmount  = 0.02f;

        // Tone network (DS-1 is bright & cutting)
        toneLP.setCut(5500.f, sr);
        toneHP.setCut(900.f, sr);

        setDrive(0.5f);
        setTone(0.5f);
        setLevel(0.5f);
    }

    void reset() override {
        stage.reset();
        toneLP.reset();
        toneHP.reset();
    }

    float process(float x) override {
        // --- Drive mapping ---
        float dB = 60.f * drive01 * drive01;
        stage.drive = std::pow(10.f, dB / 20.f);

        // --- Core nonlinear stage ---
        float y = stage.process(x);

        // --- Tone control ---
        float dark   = toneLP.processLP(y);
        float bright = toneHP.processHP(y);
        y = (1.f - tone01) * dark + tone01 * bright;

        // --- Level ---
        float levelDB = -24.f + 30.f * level01;
        float gain = std::pow(10.f, levelDB / 20.f);

        return y * gain;
    }

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "Boss DS-1"; }

private:
    float sr = 44100.f;

    AnalogStage stage;
    Waveshaper shaper;

    // Tone section
    TPT1Pole toneLP;
    TPT1Pole toneHP;

    // UI params
    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;
};

class OCDCircuit : public DistortionCircuit {
public:
    void prepare(float sampleRate) override {
        sr = sampleRate;

        // OCD uses hard-ish MOSFET clipping with amp-like response
        shaper.setMode(Waveshaper::WS_ARCCLIP);

        stage.init(sr, &shaper);

        // OCD voicing: open lows, strong punch, moderate compression
        stage.couplingHP.setCut(45.f, sr);
        stage.envLP.setCut(14.f, sr);
        stage.storageLP.setCut(5.f, sr);

        stage.baseBias   = 0.f;
        stage.biasAmount = 0.12f;
        stage.sagAmount  = 0.10f;

        // Tone network
        toneLP.setCut(9000.f, sr);
        toneHP.setCut(650.f, sr);

        // HP/LP toggle emulation: default HP mode
        hpMode = true;

        setDrive(0.5f);
        setTone(0.5f);
        setLevel(0.5f);
    }

    void reset() override {
        stage.reset();
        toneLP.reset();
        toneHP.reset();
    }

    float process(float x) override {
        // Drive: 0 → +50 dB
        float dB = 50.f * drive01 * drive01;
        stage.drive = std::pow(10.f, dB / 20.f);

        float y = stage.process(x);

        // OCD tone shaping
        float dark   = toneLP.processLP(y);
        float bright = toneHP.processHP(y);

        // HP / LP voicing switch
        float t = tone01;
        if (hpMode)
            y = (1.f - t) * dark + t * bright;
        else
            y = (1.f - t) * bright + t * dark;

        // Level: -18 → +12 dB
        float levelDB = -18.f + 30.f * level01;
        float gain = std::pow(10.f, levelDB / 20.f);

        return y * gain;
    }

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    void setHPMode(bool on) { hpMode = on; }

    std::string name() const override { return "Fulltone OCD"; }

private:
    float sr = 44100.f;

    AnalogStage stage;
    Waveshaper shaper;

    TPT1Pole toneLP;
    TPT1Pole toneHP;

    bool hpMode = true;

    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;
};


class RATCircuit : public DistortionCircuit {
public:
    void prepare(float sampleRate) override {
        sr = sampleRate;

        // RAT core: op-amp + hard-ish diodes
        shaper.setMode(Waveshaper::WS_ARCCLIP);

        stage.init(sr, &shaper);

        // RAT feel defaults
        stage.couplingHP.setCut(30.f, sr);
        stage.envLP.setCut(14.f, sr);
        stage.storageLP.setCut(5.f, sr);

        stage.baseBias   = 0.f;
        stage.biasAmount = 0.20f;
        stage.sagAmount  = 0.15f;

        // RAT "Filter" control (reverse tone)
        toneLP.setCut(6000.f, sr);
        toneHP.setCut(600.f, sr);

        setDrive(0.5f);
        setTone(0.5f);
        setLevel(0.5f);
    }

    void reset() override {
        stage.reset();
        toneLP.reset();
        toneHP.reset();
    }

    float process(float x) override {
        // Drive: 0 → +55 dB
        float dB = 55.f * drive01 * drive01;
        stage.drive = std::pow(10.f, dB / 20.f);

        // Core distortion
        float y = stage.process(x);

        // RAT tone: inverted brightness control
        float dark   = toneLP.processLP(y);
        float bright = toneHP.processHP(y);
        float t = 1.f - tone01;  // RAT filter works backwards
        y = (1.f - t) * dark + t * bright;

        // Level: -24 → +6 dB
        float levelDB = -24.f + 30.f * level01;
        float gain = std::pow(10.f, levelDB / 20.f);

        return y * gain;
    }

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "ProCo RAT"; }

private:
    float sr = 44100.f;

    AnalogStage stage;
    Waveshaper shaper;

    TPT1Pole toneLP;
    TPT1Pole toneHP;

    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;
};

class ToneBenderCircuit : public DistortionCircuit {
public:
    void prepare(float sampleRate) override {
        sr = sampleRate;

        // Tone Bender: very asymmetric, transistor-heavy clipping
        shaper.setMode(Waveshaper::WS_TRIODE);

        stage.init(sr, &shaper);

        // Tone Bender voicing: aggressive mid focus, unstable bias
        stage.couplingHP.setCut(8.f, sr);
        stage.envLP.setCut(5.f, sr);
        stage.storageLP.setCut(1.5f, sr);

        stage.baseBias   = -0.22f;
        stage.biasAmount = 0.55f;
        stage.sagAmount  = 0.35f;

        // Tone Bender has very little tone shaping — mostly raw
        toneLP.setCut(10000.f, sr);

        setDrive(0.5f);
        setTone(0.5f);
        setLevel(0.5f);
    }

    void reset() override {
        stage.reset();
        toneLP.reset();
    }

    float process(float x) override {
        // Drive: extreme (0 → +70 dB)
        float dB = 70.f * drive01 * drive01;
        stage.drive = std::pow(10.f, dB / 20.f);

        // Core fuzz
        float y = stage.process(x);

        // Tone (simple brightness control)
        float dark = toneLP.processLP(y);
        y = (1.f - tone01) * dark + tone01 * y;

        // Level: -24 → +6 dB
        float levelDB = -24.f + 30.f * level01;
        float gain = std::pow(10.f, levelDB / 20.f);

        return y * gain;
    }

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "Tone Bender"; }

private:
    float sr = 44100.f;

    AnalogStage stage;
    Waveshaper shaper;

    TPT1Pole toneLP;

    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;
};


struct TransistorFuzzCircuit : public DistortionCircuit
{
    float sr = 44100.f;

    Waveshaper  shaper;
    AnalogStage stage;

    TPT1Pole preHP;     // input coupling / bass trim
    TPT1Pole preLP;     // bandwidth limit into fuzz
    TPT1Pole postLP;    // tone shaping

    // user controls
    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;

    // smoothed controls
    float driveLP = 0.5f;
    float toneLP  = 0.5f;
    float levelLP = 0.5f;

    // DC blocker
    float dc_x1 = 0.f, dc_y1 = 0.f;

    // ----------------------------------------------------

    void prepare(float sampleRate) override
    {
        sr = sampleRate;
        reset();

        stage.init(sr, &shaper);

        // Fuzz-style defaults
        stage.couplingHP.setCut(20.f, sr);
        stage.envLP.setCut(25.f, sr);
        stage.storageLP.setCut(1.5f, sr);

        stage.baseBias   = 0.1f;    // transistor idle bias
        stage.biasAmount = 0.15f;   // very strong bias shift
        stage.sagAmount  = 0.25f;   // extreme supply sag

        preHP.setCut(70.f, sr);
        preLP.setCut(4500.f, sr);
        postLP.setCut(3500.f, sr);

        // Aggressive nonlinearity
        shaper.setMode(Waveshaper::WS_HARDCLIP);
        shaper.setDrive(1.f);
        shaper.setCurve(1.f);
        shaper.setOutGain(1.f);
        shaper.setAM(1.f);
    }

    void reset() override
    {
        stage.reset();
        preHP.reset();
        preLP.reset();
        postLP.reset();

        driveLP = drive01;
        toneLP  = tone01;
        levelLP = level01;

        dc_x1 = dc_y1 = 0.f;
    }

    // ----------------------------------------------------

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "Transistor Fuzz"; }

    // ----------------------------------------------------

    inline float zap(float x) const
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    inline float dcBlock(float x)
    {
        float y = x - dc_x1 + 0.995f * dc_y1;
        dc_x1 = x; dc_y1 = y;
        return y;
    }

    // ----------------------------------------------------

    float process(float x) override
    {
        // smooth knobs
        driveLP += 0.002f * (drive01 - driveLP);
        toneLP  += 0.002f * (tone01  - toneLP);
        levelLP += 0.002f * (level01 - levelLP);

        // ---------------------------
        // Control mapping
        // ---------------------------

        // Drive: 0..+60 dB (fuzz is extreme)
        float dB = 60.f * (driveLP * driveLP);
        stage.drive = std::pow(10.f, dB / 20.f);

        // Tone: moves both preLP & postLP (classic fuzz tone feel)
        float preHz  = 3000.f * std::pow(10.f, std::log10(9000.f / 3000.f) * toneLP);
        float postHz = 1200.f * std::pow(10.f, std::log10(6000.f / 1200.f) * toneLP);
        preLP.setCut(preHz, sr);
        postLP.setCut(postHz, sr);

        // Level: -36..+6 dB
        float leveldB = -36.f + 42.f * levelLP;
        float outGain = std::pow(10.f, leveldB / 20.f);

        // ---------------------------
        // Circuit flow
        // ---------------------------

        x = stage.couplingHP.processHP(x);
        x = preHP.processHP(x);
        x = preLP.processLP(x);

        float env = stage.envLP.processLP(std::fabs(x));

        float targetBias = stage.baseBias - stage.biasAmount * env;
        stage.bias = stage.storageLP.processLP(targetBias);
        stage.bias = zap(stage.bias);

        float sag = 1.f / (1.f + stage.sagAmount * env);

        float y = shaper.process((x * stage.drive + stage.bias) * sag);

        y = postLP.processLP(y);

        return dcBlock(zap(y * outGain));
    }
};

// TubeOverdriveCircuit.hpp
// assumes: DistortionCircuit, TPT1Pole, Waveshaper, AnalogStage exist
// Notes:
// - Uses AnalogStage for coupling HP + envelope + bias memory + sag.
// - Uses Waveshaper in a “triode-ish” mode for the core nonlinearity.
// - Uses simple pre/post filtering for “tone” (bright ↔ dark).
// - Includes output DC block + parameter smoothing.

struct TubeOverdriveCircuit : public DistortionCircuit
{
    float sr = 44100.f;

    Waveshaper  shaper;   // nonlinear core (triode-ish)
    AnalogStage stage;    // couplingHP, envLP, storageLP, bias, sag, drive

    // tone shaping
    TPT1Pole preHP;   // tightens low end into the drive stage (guitar amp-ish)
    TPT1Pole preLP;   // bandwidth limiting before clipping
    TPT1Pole postLP;  // tone control smoothing after clipping

    // user knobs (0..1)
    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;

    // smoothed knobs
    float driveLP = 0.5f;
    float toneLP  = 0.5f;
    float levelLP = 0.5f;

    // output DC block
    float dc_x1 = 0.f, dc_y1 = 0.f;

    // -------------------------------------------------

    void prepare(float sampleRate) override
    {
        sr = sampleRate;
        reset();

        stage.init(sr, &shaper);

        // “tube-ish” circuit defaults
        stage.couplingHP.setCut(25.f, sr);   // coupling cap
        stage.envLP.setCut(15.f, sr);        // envelope follower
        stage.storageLP.setCut(2.5f, sr);    // bias memory (slow)

        stage.baseBias   = 0.f;
        stage.biasAmount = 0.06f;              // stronger than diode: tube bias shift
        stage.sagAmount  = 0.10f;              // tube supply sag feel

        // pre/post filters
        preHP.setCut(90.f, sr);              // keep lows from flubbing out
        preLP.setCut(9000.f, sr);            // tame harsh HF into clip
        postLP.setCut(6500.f, sr);           // default tone

        // Choose a triode-ish curve (your earlier Waveshaper has WS_TRIODE)
        shaper.setMode(Waveshaper::WS_TRIODE);
        shaper.setDrive(1.f);
        shaper.setCurve(1.f);
        shaper.setOutGain(1.f);
        shaper.setAM(1.f);
    }

    void reset() override
    {
        stage.reset();
        preHP.reset();
        preLP.reset();
        postLP.reset();

        driveLP = drive01;
        toneLP  = tone01;
        levelLP = level01;

        dc_x1 = dc_y1 = 0.f;
    }

    // -------------------------------------------------

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "Tube Overdrive"; }

    // -------------------------------------------------

    inline float zap(float x) const
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    inline float dcBlock(float x)
    {
        float y = x - dc_x1 + 0.995f * dc_y1;
        dc_x1 = x; dc_y1 = y;
        return y;
    }

    // -------------------------------------------------

    float process(float x) override
    {
        // smooth knobs
        driveLP += 0.002f * (drive01 - driveLP);
        toneLP  += 0.002f * (tone01  - toneLP);
        levelLP += 0.002f * (level01 - levelLP);

        // -------------------------------------------------
        // knob mapping
        // -------------------------------------------------

        // Drive: 0..+45 dB (tube is typically a little less extreme than diode/fuzz)
        // square law gives finer control in low/mid drive
        const float dB = 45.f * (driveLP * driveLP);
        stage.drive = std::pow(10.f, dB / 20.f);

        // Tone: darker -> lower postLP cutoff, brighter -> higher cutoff
        // log sweep 1800..12000 Hz
        const float lo = 1800.f;
        const float hi = 12000.f;
        const float postHz = lo * std::pow(10.f, std::log10(hi / lo) * toneLP);
        postLP.setCut(postHz, sr);

        // Also modulate preLP slightly with tone (brighter = more bandwidth into clip)
        const float preLo = 5500.f;
        const float preHi = 12000.f;
        const float preHz = preLo * std::pow(10.f, std::log10(preHi / preLo) * toneLP);
        preLP.setCut(preHz, sr);

        // Level: -30..+6 dB (tube OD often needs more cut)
        const float leveldB = -30.f + 36.f * levelLP;
        const float outGain = std::pow(10.f, leveldB / 20.f);

        // -------------------------------------------------
        // circuit flow
        // -------------------------------------------------

        // input coupling (DC block) + low-end tightening
        x = stage.couplingHP.processHP(x);
        x = preHP.processHP(x);

        // bandwidth into the nonlinear stage
        x = preLP.processLP(x);

        // envelope follower
        const float env = stage.envLP.processLP(std::fabs(x));

        // dynamic bias shift (tube grid bias moves with signal)
        const float targetBias = stage.baseBias - stage.biasAmount * env;
        stage.bias = stage.storageLP.processLP(targetBias);
        stage.bias = zap(stage.bias);

        // sag (supply compression)
        const float sag = 1.f / (1.f + stage.sagAmount * env);

        // nonlinear core (triode-ish)
        float y = shaper.process((x * stage.drive + stage.bias) * sag);

        // post smoothing / tone
        y = postLP.processLP(y);

        // output DC safety + level
        return dcBlock(zap(y * outGain));
    }
};

class TubeScreamerCircuit : public DistortionCircuit {
public:
    void prepare(float sampleRate) override {
        sr = sampleRate;

        // Device model
        shaper.setMode(Waveshaper::WS_SOFTEXP);

        // Core analog stage
        stage.init(sr, &shaper);

        // TS-9 style defaults
        stage.couplingHP.setCut(100.f, sr);
        stage.envLP.setCut(18.f, sr);
        stage.storageLP.setCut(6.f, sr);

        stage.baseBias   = 0.f;
        stage.biasAmount = 0.15f;
        stage.sagAmount  = 0.08f;

        // Tone network (post stage)
        toneLP.setCut(1800.f, sr);
        toneHP.setCut(720.f, sr);

        setDrive(0.5f);
        setTone(0.5f);
        setLevel(0.5f);
    }

    void reset() override {
        stage.reset();
        toneLP.reset();
        toneHP.reset();
    }

    float process(float x) override {
        // --- Drive mapping ---
        float dB = 40.f * drive01 * drive01;
        stage.drive = std::pow(10.f, dB / 20.f);

        // --- Core nonlinear stage ---
        float y = stage.process(x);

        // --- Tone control ---
        float dark   = toneLP.processLP(y);
        float bright = toneHP.processHP(y);
        y = (1.f - tone01) * dark + tone01 * bright;

        // --- Level ---
        float levelDB = -18.f + 36.f * level01;
        float gain = std::pow(10.f, levelDB / 20.f);

        return y * gain;
    }

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "Tube Screamer TS-9"; }

private:
    float sr = 44100.f;

    AnalogStage stage;
    Waveshaper shaper;

    // Tone section
    TPT1Pole toneLP;
    TPT1Pole toneHP;

    // UI params
    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;
};



// ------------------ tiny smoothing helper ------------------
struct Slew {
    float y = 0.f;
    float a = 0.001f; // smoothing coefficient

    void setTimeMs(float timeMs, float sr) {
        // 1-pole smoothing: a = exp(-1/(tau*sr))
        float t = std::max(0.1f, timeMs) * 0.001f;
        a = std::exp(-1.f / (t * std::max(1.f, sr)));
    }

    void reset(float v) { y = v; }

    float process(float target) {
        y = a * y + (1.f - a) * target;
        return y;
    }
};

// ------------------ click-free crossfade switch ------------------
struct XFade {
    int  n = 0;
    int  i = 0;

    void start(int samples) {
        n = std::max(1, samples);
        i = 0;
    }
    bool active() const { return i < n; }

    // 0 -> 1
    float next() {
        float t = (n <= 1) ? 1.f : (float)i / (float)(n - 1);
        // equal-power crossfade for constant energy
        float w = std::sin(0.5f * 3.14159265359f * t);
        ++i;
        return w;
    }
};

// ------------------ DistortionEngine ------------------
class DistortionEngine {
public:
    explicit DistortionEngine(float sampleRate = 44100.f)
    : sr(sampleRate)
    {
        setSampleRate(sampleRate);
        setSmoothingMs(10.f);     // good default for automation
        setSwitchFadeMs(8.f);     // short, click-free
        reset();
    }

    void setSampleRate(float sampleRate) {
        sr = std::max(8000.f, sampleRate);
        driveSlew.setTimeMs(smoothMs, sr);
        toneSlew .setTimeMs(smoothMs, sr);
        levelSlew.setTimeMs(smoothMs, sr);
        switchFadeSamples = msToSamples_(switchFadeMs);
        if (current) current->prepare(sr);
        if (next)    next->prepare(sr);
    }

    void setSmoothingMs(float ms) {
        smoothMs = std::clamp(ms, 0.1f, 200.f);
        driveSlew.setTimeMs(smoothMs, sr);
        toneSlew .setTimeMs(smoothMs, sr);
        levelSlew.setTimeMs(smoothMs, sr);
    }

    void setSwitchFadeMs(float ms) {
        switchFadeMs = std::clamp(ms, 0.5f, 50.f);
        switchFadeSamples = msToSamples_(switchFadeMs);
    }

    void reset() {
        driveTarget = 0.5f;
        toneTarget  = 0.5f;
        levelTarget = 0.5f;

        driveSlew.reset(driveTarget);
        toneSlew .reset(toneTarget);
        levelSlew.reset(levelTarget);

        if (current) current->reset();
        if (next)    next->reset();

        xfade = XFade{};
    }

    // ----- set active circuit immediately (no crossfade) -----
    void setCircuitImmediate(DistortionCircuit* c) {
        current = std::move(c);
        next->reset();
        xfade = XFade{};
        if (current) {
            current->prepare(sr);
            current->reset();
            // apply current params
            current->setDrive(driveSlew.y);
            current->setTone(toneSlew.y);
            current->setLevel(levelSlew.y);
        }
    }

    // ----- request a circuit switch (crossfaded) -----
    void switchCircuit(DistortionCircuit* c, bool resetNew = true) {
        next = std::move(c);
        if (!next) return;

        next->prepare(sr);
        if (resetNew) next->reset();

        // Start the new circuit with the *current smoothed* params so it matches immediately
        next->setDrive(driveSlew.y);
        next->setTone(toneSlew.y);
        next->setLevel(levelSlew.y);

        xfade.start(switchFadeSamples);
    }

    // ----- UI knobs 0..1 -----
    void setDrive(float v01) { driveTarget = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) { toneTarget  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) { levelTarget = std::clamp(v01, 0.f, 1.f); }

    // Optional: expose instantaneous smoothed values
    float drive() const { return driveSlew.y; }
    float tone()  const { return toneSlew.y; }
    float level() const { return levelSlew.y; }

    // ----- processing -----
    float process(float x) {
        // 1) Smooth params
        float d = driveSlew.process(driveTarget);
        float t = toneSlew .process(toneTarget);
        float l = levelSlew.process(levelTarget);

        // 2) Push params into circuits
        if (current) {
            current->setDrive(d);
            current->setTone(t);
            current->setLevel(l);
        }
        if (next) {
            next->setDrive(d);
            next->setTone(t);
            next->setLevel(l);
        }

        // 3) Process with optional crossfade
        if (!current && next) {
            // if no current, promote next immediately
            current = std::move(next);
            xfade = XFade{};
            return current ? current->process(x) : x;
        }

        if (current && next && xfade.active()) {
            float yA = current->process(x);
            float yB = next->process(x);
            float w  = xfade.next();   // 0..1, equal-power
            float out = (1.f - w) * yA + w * yB;

            // if crossfade finished, promote next
            if (!xfade.active()) {
                current = std::move(next);
                next->reset();
            }
            return out;
        }

        return current ? current->process(x) : x;
    }

    void run(const float* in, float* out, size_t n) {
        for (size_t i = 0; i < n; ++i) out[i] = process(in[i]);
    }

    bool hasCircuit() const { return (bool)current; }

private:
    int msToSamples_(float ms) const {
        return (int)std::max(1.f, std::round(ms * 0.001f * sr));
    }

    float sr = 44100.f;

    // smoothing + switching settings
    float smoothMs = 10.f;
    float switchFadeMs = 8.f;
    int   switchFadeSamples = 256;

    // targets
    float driveTarget = 0.5f;
    float toneTarget  = 0.5f;
    float levelTarget = 0.5f;

    // smoothed
    Slew driveSlew, toneSlew, levelSlew;

    // circuits
    DistortionCircuit* current;
    DistortionCircuit* next;
    // crossfade
    XFade xfade;
};
