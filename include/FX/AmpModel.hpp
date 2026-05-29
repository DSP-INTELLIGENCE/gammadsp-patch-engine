// GDSP_AmpModel.hpp
#pragma once
#include <cmath>
#include <algorithm>

// ------------------ Simple Biquad ------------------
struct Biquad {
    float b0=1, b1=0, b2=0, a1=0, a2=0;
    float z1=0, z2=0;

    void reset() { z1 = z2 = 0.f; }

    float process(float x) {
        float y = b0*x + z1;
        z1 = b1*x - a1*y + z2;
        z2 = b2*x - a2*y;
        return y;
    }

    // RBJ cookbook helpers
    static Biquad lowShelf(float sr, float f0, float dBgain, float S=1.0f) {
        Biquad q;
        f0 = std::clamp(f0, 20.f, 0.45f*sr);
        float A = std::pow(10.f, dBgain/40.f);
        float w0 = 2.f * 3.14159265359f * f0 / sr;
        float cw = std::cos(w0), sw = std::sin(w0);
        float alpha = sw/2.f * std::sqrt((A + 1.f/A) * (1.f/S - 1.f) + 2.f);

        float b0 =    A*((A+1) - (A-1)*cw + 2*std::sqrt(A)*alpha);
        float b1 =  2*A*((A-1) - (A+1)*cw);
        float b2 =    A*((A+1) - (A-1)*cw - 2*std::sqrt(A)*alpha);
        float a0 =        (A+1) + (A-1)*cw + 2*std::sqrt(A)*alpha;
        float a1 =   -2*((A-1) + (A+1)*cw);
        float a2 =        (A+1) + (A-1)*cw - 2*std::sqrt(A)*alpha;

        q.b0 = b0/a0; q.b1 = b1/a0; q.b2 = b2/a0;
        q.a1 = a1/a0; q.a2 = a2/a0;
        return q;
    }

    static Biquad highShelf(float sr, float f0, float dBgain, float S=1.0f) {
        Biquad q;
        f0 = std::clamp(f0, 20.f, 0.45f*sr);
        float A = std::pow(10.f, dBgain/40.f);
        float w0 = 2.f * 3.14159265359f * f0 / sr;
        float cw = std::cos(w0), sw = std::sin(w0);
        float alpha = sw/2.f * std::sqrt((A + 1.f/A) * (1.f/S - 1.f) + 2.f);

        float b0 =    A*((A+1) + (A-1)*cw + 2*std::sqrt(A)*alpha);
        float b1 = -2*A*((A-1) + (A+1)*cw);
        float b2 =    A*((A+1) + (A-1)*cw - 2*std::sqrt(A)*alpha);
        float a0 =        (A+1) - (A-1)*cw + 2*std::sqrt(A)*alpha;
        float a1 =    2*((A-1) - (A+1)*cw);
        float a2 =        (A+1) - (A-1)*cw - 2*std::sqrt(A)*alpha;

        q.b0 = b0/a0; q.b1 = b1/a0; q.b2 = b2/a0;
        q.a1 = a1/a0; q.a2 = a2/a0;
        return q;
    }

    static Biquad peaking(float sr, float f0, float Q, float dBgain) {
        Biquad q;
        f0 = std::clamp(f0, 20.f, 0.45f*sr);
        Q  = std::max(0.1f, Q);
        float A = std::pow(10.f, dBgain/40.f);
        float w0 = 2.f * 3.14159265359f * f0 / sr;
        float cw = std::cos(w0), sw = std::sin(w0);
        float alpha = sw/(2.f*Q);

        float b0 = 1.f + alpha*A;
        float b1 = -2.f*cw;
        float b2 = 1.f - alpha*A;
        float a0 = 1.f + alpha/A;
        float a1 = -2.f*cw;
        float a2 = 1.f - alpha/A;

        q.b0 = b0/a0; q.b1 = b1/a0; q.b2 = b2/a0;
        q.a1 = a1/a0; q.a2 = a2/a0;
        return q;
    }
};

// ------------------ Tone stack (amp-like 3-band) ------------------
struct ToneStack3Band {
    float sr = 44100.f;

    // 0..1 UI
    float bass01=0.5f, mid01=0.5f, treble01=0.5f;

    Biquad bass, mid, treble;

    void setSampleRate(float s) { sr = std::max(8000.f, s); update(); }
    void reset() { bass.reset(); mid.reset(); treble.reset(); }

    static float knobToDb(float k, float rangeDb) {
        // map 0..1 to -range..+range with a little center softness
        float x = (k - 0.5f) * 2.f;          // -1..+1
        x = std::copysign(x*x, x);           // gentle taper
        return x * rangeDb;
    }

    void update() {
        float bassDb   = knobToDb(bass01,   12.f);  // ±12 dB
        float midDb    = knobToDb(mid01,    10.f);  // ±10 dB
        float trebleDb = knobToDb(treble01, 12.f);  // ±12 dB

        bass   = Biquad::lowShelf (sr, 120.f,  bassDb,   0.9f);
        mid    = Biquad::peaking  (sr, 750.f,  0.8f,     midDb);
        treble = Biquad::highShelf(sr, 3200.f, trebleDb, 0.9f);
    }

    float process(float x) {
        x = bass.process(x);
        x = mid.process(x);
        x = treble.process(x);
        return x;
    }
};

// ------------------ Cab rolloff (simple LPF-ish) ------------------
struct OnePoleLP {
    float a=0, z=0;
    void setCutHz(float hz, float sr){
        float fc = std::clamp(hz, 50.f, 0.45f*sr);
        a = std::exp(-2.f * 3.14159265359f * fc / sr);
    }
    void reset(){ z = 0.f; }
    float process(float x){
        z = a*z + (1.f - a)*x;
        return z;
    }
};

// ------------------ Amp Model wiring ------------------
// Forward declare your existing classes:
class DS1;
class TS9;
struct TriodeStage;
class Triode2Stage;

class AmpModel {
public:
    enum BoostMode { BOOST_OFF, BOOST_TS9, BOOST_DS1 };

    AmpModel(float sampleRate, DS1* ds1, TS9* ts9, TriodeStage* pre, Triode2Stage* power)
    : sr(sampleRate), ds1(ds1), ts9(ts9), pre(pre), power(power)
    {
        cabLP.setCutHz(6500.f, sr);
        tone.setSampleRate(sr);
        setBoostMode(BOOST_OFF);
        setGain(0.5f);
        setMaster(0.5f);
        setLevel(0.5f);
        setSag(0.35f);
        setNFB(0.2f);
    }

    void setSampleRate(float sampleRate){
        sr = std::max(8000.f, sampleRate);
        tone.setSampleRate(sr);
        cabLP.setCutHz(cabCutHz, sr);
        // power stage SR set handled externally if needed
    }

    // ---- Amp knobs (0..1) ----
    void setBoostMode(BoostMode m) { boost = m; }
    void setGain(float v01)   { gain01   = std::clamp(v01, 0.f, 1.f); }
    void setMaster(float v01) { master01 = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01)  { level01  = std::clamp(v01, 0.f, 1.f); }

    void setBass(float v01)   { tone.bass01 = std::clamp(v01,0.f,1.f); tone.update(); }
    void setMid(float v01)    { tone.mid01 = std::clamp(v01,0.f,1.f);  tone.update(); }
    void setTreble(float v01) { tone.treble01 = std::clamp(v01,0.f,1.f); tone.update(); }

    // power feel
    void setSag(float v01) { sag01 = std::clamp(v01,0.f,1.f); }
    void setNFB(float v01) { nfb01 = std::clamp(v01,0.f,1.f); }

    // cab
    void setCabCutHz(float hz){ cabCutHz = std::clamp(hz, 1500.f, 12000.f); cabLP.setCutHz(cabCutHz, sr); }
    void setCabEnabled(bool e){ cabEnabled = e; }

    void reset(){
        tone.reset();
        cabLP.reset();
        // reset ds1/ts9/pre/power externally if they have state
    }

    float process(float x){
        // ---- optional boost/pedal in front ----
        if(boost == BOOST_TS9 && ts9) {
            // TS9: set it like a boost: high level, moderate drive (you can expose separately)
            // (Assumes TS9 has setDrive/setTone/setLevel used elsewhere)
            x = ts9->process(x);
        } else if(boost == BOOST_DS1 && ds1) {
            x = ds1->process(x);
        }

        // ---- preamp (TriodeStage) ----
        if(pre){
            pre->drive = gain01;   // gain knob maps to preamp drive
            x = pre->process(x);
        }

        // ---- tone stack ----
        x = tone.process(x);

        // ---- master into power amp ----
        float masterDB = 45.f * (master01 * master01); // 0..+45 dB
        float masterGain = std::pow(10.f, masterDB/20.f);
        float driveIn = x * masterGain;

        if(power){
            // Map sag/nfb to your Triode2Stage setters
            // (Assumes you have these; otherwise remove and wire to your param system)
            power->setSag(sag01);
            power->setNegFeedback(0.05f + 0.90f * nfb01); // avoid 0 exactly, cap at ~0.95
            x = power->process(driveIn);
        } else {
            x = driveIn;
        }

        // ---- cab rolloff (optional) ----
        if(cabEnabled) x = cabLP.process(x);

        // ---- final level ----
        float levelDB = -24.f + 30.f * level01; // -24..+6
        float outGain = std::pow(10.f, levelDB/20.f);
        return x * outGain;
    }

    void run(const float* in, float* out, size_t n){
        for(size_t i=0;i<n;i++) out[i] = process(in[i]);
    }

private:
    float sr = 44100.f;

    DS1* ds1 = nullptr;
    TS9* ts9 = nullptr;
    TriodeStage* pre = nullptr;
    Triode2Stage* power = nullptr;

    BoostMode boost = BOOST_OFF;

    float gain01   = 0.5f;
    float master01 = 0.5f;
    float level01  = 0.5f;

    float sag01 = 0.35f;
    float nfb01 = 0.2f;

    ToneStack3Band tone;

    bool cabEnabled = true;
    float cabCutHz = 6500.f;
    OnePoleLP cabLP;
};
