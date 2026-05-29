#pragma once
#include <cmath>
#include <algorithm>

// ---------------------------------------------
// Stereo Tape Delay
// ---------------------------------------------
class EchoMachine: public StereoFunction {
public:
    EchoMachine(float maxDelaySeconds = 2.0f)
    : delayL(maxDelaySeconds, 0.25f),
      delayR(maxDelaySeconds, 0.251f),
      toneL(6000.0f),
      toneR(6000.0f),
      wowL(0.2f), wowR(0.2f),
      flutterL(6.0f), flutterR(6.0f)
    {
        delayL.setIpolType(gam::ipl::CUBIC);
        delayR.setIpolType(gam::ipl::CUBIC);

        wowR.phaseAdd(float(M_PI_2));
        flutterR.phaseAdd(float(M_PI_2));

        setTime(0.35f);
        setFeedback(0.55f);
        setMix(0.45f);
        setTone(6000.0f);
        setDrive(0.4f);

        setWow(0.25f, 0.003f);
        setFlutter(6.0f, 0.0006f);
    }

    // ---------------- Parameters ----------------

    void setTime(float sec)
    {
        baseDelay = std::clamp(sec, 0.001f, delayL.getMaxDelayTime());
    }

    void setFeedback(float fb)
    {
        feedback = std::clamp(fb, 0.0f, 0.95f);
    }

    void setMix(float m)
    {
        mix = std::clamp(m, 0.0f, 1.0f);
    }

    void setTone(float cutoffHz)
    {
        cutoffHz = std::clamp(cutoffHz, 50.0f, 0.45f * (float)gam::sampleRate());
        toneL.setFreq(cutoffHz);
        toneR.setFreq(cutoffHz);
    }

    void setDrive(float d)
    {
        drive = std::max(0.0f, d);
    }

    void setWow(float rateHz, float depthSec)
    {
        wowRate  = std::max(0.01f, rateHz);
        wowDepth = std::max(0.0f, depthSec);

        wowL.setFreq(wowRate);
        wowR.setFreq(wowRate);
    }

    void setFlutter(float rateHz, float depthSec)
    {
        flutterRate  = std::max(0.1f, rateHz);
        flutterDepth = std::max(0.0f, depthSec);

        flutterL.setFreq(flutterRate);
        flutterR.setFreq(flutterRate);
    }

    // ---------------- DSP ----------------
    inline StereoSample process(const StereoSample& in) {
        return processStereo(in.L,in.R);
    }
    inline StereoSample processStereo(float inL, float inR)
    {
        // ---- Modulated delay times ----
        float modL =
            wowDepth     * wowL.process() +
            flutterDepth * flutterL.process();

        float modR =
            wowDepth     * wowR.process() +
            flutterDepth * flutterR.process();

        float dL = baseDelay * (1.0f + modL);
        float dR = baseDelay * (1.0f + modR);

        // safety clamp (critical)
        float minDelay = 0.0005f; // 0.5 ms
        float maxDelay = delayL.getMaxDelayTime();

        dL = std::clamp(dL, minDelay, maxDelay);
        dR = std::clamp(dR, minDelay, maxDelay);

        // ~2ms smoothing time constant
        float sr = (float)gam::sampleRate();
        float g  = 1.f - std::exp(-1.f / (0.002f * sr));

        dLs += g * (dL - dLs);
        dRs += g * (dR - dRs);

        delayL.setDelay(dLs);
        delayR.setDelay(dRs);

        delayL.setDelay(dL);
        delayR.setDelay(dR);

        // ---- Read current delay ----
        float dl = delayL.read();
        float dr = delayR.read();

        // ---- Tape saturation (soft, level-dependent) ----
        float satL = softSat(dl);
        float satR = softSat(dr);

        satL = dcL.process(satL);
        satR = dcL.process(satR);

        // ---- HF loss (tape damping) ----
        float fbL = toneL.process(satL);
        float fbR = toneR.process(satR);

        // ---- Write with feedback ----
        float fbSafe = feedback * (1.0f - 0.25f * (wowDepth + flutterDepth));
        fbSafe = std::clamp(fbSafe, 0.0f, 0.95f);

        
        delayL.write(inL + (fbL + cross * fbR) * fbSafe);
        delayR.write(inR + (fbR + cross * fbL) * fbSafe);

        // ---- Equal-power dry/wet ----
        float theta = mix * float(M_PI_2);
        float dryG  = std::cos(theta);
        float wetG  = std::sin(theta);
        
        L = inL * dryG + dl * wetG;
        R = inR * dryG + dr * wetG;
        return StereoSample(L,R);

    }

    Modulator m_delay, m_feedback, m_mix, m_drive, m_wow_rate, m_wow_depth, m_flutter_rate, m_flutter_depth;
    Function * p_in_tap = nullptr;
    Function * p_out_tap = nullptr;
    
    ModDelay& getDelayL() { return delayL; }
    ModDelay& getDelayR() { return delayR; }
    LFO&      getWowL() { return wowL; }
    LFO&      getWowR() { return wowR; }
    LFO&      getFlutterL() { return flutterL; }
    LFO&      getFlutterR() { return flutterR; }

private:
    // Soft tape-style saturation
    inline float softSat(float x) const
    {
        if (drive <= 0.0f) return x;
        float k = 1.0f + drive * 8.0f;
        return std::tanh(x * k) / std::tanh(k);
    }

    
private:
    // Delay lines
    ModDelay delayL;
    ModDelay delayR;

    // Feedback tone shaping
    OnePole toneL;
    OnePole toneR;

    BlockDC dcL,dcR;

    // Modulation
    LFO wowL, wowR;
    LFO flutterL, flutterR;
    
    // Parameters
    float baseDelay   = 0.35f;
    float feedback    = 0.55f;
    float mix         = 0.45f;
    float drive       = 0.4f;

    float wowRate     = 0.25f;
    float wowDepth    = 0.003f;

    float flutterRate = 6.0f;
    float flutterDepth= 0.0006f;

    float dLs = 0.35f;
    float dRs = 0.351f;
    float cross = 0.15f;
    float L,R;
};
