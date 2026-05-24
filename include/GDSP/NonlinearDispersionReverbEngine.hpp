#pragma once
#include <array>
#include <algorithm>
#include <cmath>

// Uses your primitives
// ModDelay, AllPass1Block/AllPass2Block, HighPassFilter, LowPassFilter, DCReject, SoftClip, Pan (optional)

class NonlinearDispersionReverbEngine : public Function
{
public:
    NonlinearDispersionReverbEngine()
    : preDiff1(1200.f, 6),
      preDiff2(2200.f, 6)
    {
        setSize(1.0f);
        setDecay(0.65f);
        setDamp(0.35f);
        setMaterial(0.4f);
        setWidth(0.6f);
        setMix(0.35f);

        setModRate(0.08f);
        setModDepth(0.002f); // seconds, micro only

        setIM(1.f);
        setAM(1.f);

        // base delay times (seconds) - choose incommensurate values
        baseTimes = { 0.0297f, 0.0371f, 0.0411f, 0.0533f };
        updateDelayTimes();

        // filters
        for (int i = 0; i < 4; ++i) {
            hp[i].freq(40.f);
            lp[i].freq(8000.f);
        }
    }

    // -------- Macros --------
    void setSize(float v)     { size = std::clamp(v, 0.1f, 2.5f); updateDelayTimes(); }
    void setDecay(float v)    { decay = std::clamp(v, 0.f, 0.99f); }
    void setDamp(float v)     { damp = std::clamp(v, 0.f, 1.f); }
    void setMaterial(float v) { material = std::clamp(v, 0.f, 1.f); }
    void setWidth(float v)    { width = std::clamp(v, 0.f, 1.f); }
    void setMix(float v)      { mix = std::clamp(v, 0.f, 1.f); }

    void setModRate(float hz)   { modRate.set(std::max(0.001f, hz)); }
    void setModDepth(float sec) { modDepth = std::max(0.f, sec); }

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float x) override
    {
        // --- pre-diffuse / disperse ---
        float in = x * im.process();
        in = preDiff1.process(in);
        in = preDiff2.process(in);

        // --- micro modulation (stable) ---
        float r = modRate.process();
        phase += r / (float)gam::sampleRate();
        if (phase >= 1.f) phase -= 1.f;
        float lfo = std::sin(phase * 6.2831853f);

        // --- FDN: read all delay outputs first ---
        float y0 = d[0].read();
        float y1 = d[1].read();
        float y2 = d[2].read();
        float y3 = d[3].read();

        // --- Hadamard feedback mix (energy preserving) ---
        // v = H * y
        float v0 =  0.5f * ( y0 + y1 + y2 + y3);
        float v1 =  0.5f * ( y0 - y1 + y2 - y3);
        float v2 =  0.5f * ( y0 + y1 - y2 - y3);
        float v3 =  0.5f * ( y0 - y1 - y2 + y3);

        // --- damping + nonlinearity in feedback ---
        // Map damp -> LP cutoff
        float lpHz = 12000.f * std::pow(0.1f, damp);   // damp=0 -> ~12k, damp=1 -> ~1.2k-ish
        lpHz = std::clamp(lpHz, 300.f, 18000.f);

        // Map material -> drive + dispersion flavor
        float drive = 1.0f + 3.0f * material;

        for (int i = 0; i < 4; ++i) {
            lp[i].freq(lpHz);
        }

        // inject input into all lines (can be shaped by width later)
        float inj = in;

        // feedback gain
        float fb = decay;

        float fb0 = loopProcess(0, v0 * fb, drive);
        float fb1 = loopProcess(1, v1 * fb, drive);
        float fb2 = loopProcess(2, v2 * fb, drive);
        float fb3 = loopProcess(3, v3 * fb, drive);

        // --- write into delays (with micro mod on time) ---
        // modDepth in seconds; apply slightly different signs
        d[0].setDelay(delayTimes[0] + modDepth * ( 0.7f * lfo));
        d[1].setDelay(delayTimes[1] + modDepth * (-0.6f * lfo));
        d[2].setDelay(delayTimes[2] + modDepth * ( 0.5f * lfo));
        d[3].setDelay(delayTimes[3] + modDepth * (-0.4f * lfo));

        d[0].write(inj + fb0);
        d[1].write(inj + fb1);
        d[2].write(inj + fb2);
        d[3].write(inj + fb3);

        // --- output taps (stereo) ---
        // width = cross-mix decorrelation (simple but effective)
        float wetL = (y0 + y2) * 0.5f;
        float wetR = (y1 + y3) * 0.5f;

        float mid = 0.5f * (wetL + wetR);
        float side = 0.5f * (wetL - wetR);
        side *= (0.2f + 0.8f * width);

        wetL = mid + side;
        wetR = mid - side;

        // Collapse to mono output for this Function (or make a stereo version later)
        float wet = 0.5f * (wetL + wetR);

        // perceptual mix
        float dry = x  * std::cos(mix * 1.5707963f);
        float w   = wet * std::sin(mix * 1.5707963f);

        float out = (dry + w) * am.process();

        // final safety
        out = std::tanh(out * 1.2f);
        return out;
    }

private:
    float loopProcess(int i, float fbIn, float drive)
    {
        // HP + LP + sat + dc reject per loop
        float z = fbIn;

        z = dc[i].process(z);
        z = hp[i].process(z);
        z = lp[i].process(z);

        z = std::tanh(z * drive);
        return z;
    }

    void updateDelayTimes()
    {
        for (int i = 0; i < 4; ++i)
            delayTimes[i] = std::clamp(baseTimes[i] * size, 0.005f, 1.8f);
    }

private:
    // Pre-diffusion / dispersion
    AllPass1Block preDiff1;
    AllPass1Block preDiff2;

    // FDN core: 4 delay lines
    std::array<ModDelay, 4> d { ModDelay(2.0f, 0.03f), ModDelay(2.0f, 0.04f), ModDelay(2.0f, 0.05f), ModDelay(2.0f, 0.06f) };

    // loop conditioning
    std::array<DCReject, 4>      dc;
    std::array<HighPassFilter, 4> hp;
    std::array<LowPassFilter, 4>  lp;

    // params
    float size = 1.f;
    float decay = 0.6f;
    float damp = 0.3f;
    float material = 0.4f;
    float width = 0.5f;
    float mix = 0.3f;

    std::array<float, 4> baseTimes;
    std::array<float, 4> delayTimes;

    Modulator modRate;
    float modDepth = 0.0f;
    float phase = 0.f;

    Modulator im, am;
};
