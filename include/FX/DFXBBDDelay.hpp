#pragma once
#include "Engine.hpp"
#include "Parameters.hpp"
#include "BlockDC.hpp"
#include "OnePole.hpp"
#include "DFXDigitalDelay.hpp"


struct DFXBBDModel
{
    OnePole preLPF, postLPF;            
    
    // user params
    float drive      = 1.2f;
    float noise      = 0.002f;
    float mix        = 1.0f;

    // clock tracking
    float minClock   = 6000.f;    // long delay -> dark
    float maxClock   = 20000.f;   // short delay -> bright
    float maxDelay   = 0.05f;     // seconds (chorus-ish range)
    float clockHz    = 12000.f;

    // clock bleed
    float clockBleed = 0.0004f;
    float clockPhase = 0.f;

    // internal state
    float lp = 0.f;
    float hp = 0.f;
    uint32_t seed = 1;

    BlockDC dcblock;

    inline float rand01()
    {
        seed = seed * 196314165u + 907633515u;
        return float(seed & 0x7fffffff) / float(0x7fffffff);
    }

    inline void updateClockFromDelay(float delaySeconds)
    {
        float t = std::clamp(delaySeconds / maxDelay, 0.f, 1.f);
        float k = 1.f - t;     // invert: longer delay -> lower clock
        k = k * k;             // slightly curved like hardware
        clockHz = minClock + (maxClock - minClock) * k;
    }

    inline float processClockBleed()
    {
        float sr = (float)gam::sampleRate();
        clockPhase += 2.f * float(M_PI) * (clockHz / sr);
        if (clockPhase > 2.f * float(M_PI)) clockPhase -= 2.f * float(M_PI);
        return std::sin(clockPhase) * clockBleed;
    }

    inline float process(float x)
    {
        // update tone from current clock
        float cutoff = std::clamp(0.22f * clockHz, 600.0f, 16000.0f);
        preLPF.setFreq(cutoff);
        postLPF.setFreq(cutoff);

        // saturation / companding-ish
        x = std::tanh(x * drive);

        // bandwidth loss ~ clock/2
        float sr = (float)gam::sampleRate();
        float fc = std::min(clockHz * 0.5f, 0.45f * sr);
        float g  = std::exp(-2.f * float(M_PI) * fc / sr);

        lp = preLPF.process((1 - g) * x + g * lp);
        float y = lp;

        y = dcblock.process(y);

        // hiss + clock bleed
        float n = (rand01() - 0.5f) * noise;
        y += n;
        y += processClockBleed();

        // blend clean vs gbbd-colored
        return postLPF.process(x * (1.0f - mix) + y * mix);
    }
};

struct DFXBBDCompander
{
    // simple shelving filters
    float pre = 0.f;
    float de  = 0.f;

    // compander state
    float env = 0.f;

    // parameters
    float preGain   = 1.6f;
    float compAmt   = 0.35f;
    float deGain    = 0.65f;

    inline float preEmphasis(float x)
    {
        // high shelf approximation
        pre += 0.25f * (x - pre);
        return x + preGain * (x - pre);
    }

    inline float compress(float x)
    {
        float a = std::fabs(x);
        env += 0.01f * (a - env);
        float gain = 1.f / (1.f + compAmt * env);
        return x * gain;
    }

    inline float deEmphasis(float x)
    {
        de += 0.25f * (x - de);
        return x - deGain * (x - de);
    }

    inline float processPre(float x)
    {
        return compress(preEmphasis(x));
    }

    inline float processPost(float x)
    {
        return deEmphasis(x);
    }
};

class DFXBBDDelay : public Function
{
public:
    DFXBBDDelay(float maxDelay = 2.0f, float initDelay = 0.25f)
    : engine(maxDelay, initDelay)
    {
        // sensible defaults for chorus-style BBD behavior
        bbd.drive = 1.3f;
        bbd.noise = 0.0015f;
        bbd.mix   = 1.0f;

        bbd.minClock = 7000.f;
        bbd.maxClock = 20000.f;
        bbd.maxDelay = 0.05f;
        bbd.clockBleed = 0.00035f;
    }

    // mirror DigitalDelay API
    void setDelay(float s)     { engine.setDelay(s); }
    void setFeedback(float f)  { engine.setFeedback(f); }
    void setMix(float m)       { engine.setMix(m); }
    void setWet(float w)       { engine.setWet(w); }
    void setDry(float d)       { engine.setDry(d); }
    
    void setDelayMod(float v)  { engine.setDelayMod(v); }
    void setFbkMod(float v)    { engine.setFbkMod(v); }
    void setMixMod(float v)    { engine.setMixMod(v); }
    void setAM(float v)        { engine.setAM(v); }
    void setDepth(float v)     { engine.setDepth(v); }
    void setLFO(float v)       { engine.setLFO(v); }

    // BBD controls
    void setBBD(float drive, float noise, float minClock, float maxClock, float mix)
    {
        bbd.drive    = drive;
        bbd.noise    = noise;
        bbd.minClock = minClock;
        bbd.maxClock = maxClock;
        bbd.mix      = mix;
    }

    void setClockBleed(float a) { bbd.clockBleed = a; }
    void setClockMaxDelay(float s) { bbd.maxDelay = std::max(0.001f, s); }

    float process(float x) override
    {
        // pre emphasis + companding
        float y = comp.processPre(x);

        // run time engine first
        y = engine.process(y);

        // 🔥 track true modulated delay (not just base parameter)
        float actualDelay = engine.getDelayTime();
        bbd.updateClockFromDelay(actualDelay);

        
        // BBD coloration
        y = bbd.process(y);

        // de-emphasis + output filter
        y = comp.processPost(y);
        

        return y;
    }


private:
    DFXDigitalDelay engine;
    DFXBBDModel     bbd;
    DFXBBDCompander comp;
    
};
