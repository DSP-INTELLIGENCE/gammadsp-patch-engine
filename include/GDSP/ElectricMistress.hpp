#pragma once

class ElectricMistress : public Function {
public:
    ElectricMistress(float sr = 48000.f)
    : sampleRate(sr),
      delay(0.01f, 0.001f)
    {
        delay.setIpolType(gam::ipl::ALLPASS); // 🔑 critical

        lfo.setFreq(0.15f);
        lfo.setWave(LFO::SINE);
        lfo.setUnipolar(false);

        lp.freq(8000.f);
        clipDrive = 1.8f;

        baseDelay = 0.0008f;
        depth     = 0.0025f;
        feedback  = 0.75f;
        mix       = 0.5f;
    }

    // ---------- controls ----------
    void setRate(float hz)     { lfo.setFreq(std::clamp(hz, 0.01f, 1.0f)); }
    void setDepth(float sec)   { depth = std::clamp(sec, 0.f, 0.005f); }
    void setFeedback(float fb) { feedback = std::clamp(fb, 0.f, 0.95f); }
    void setMix(float m)       { mix = std::clamp(m, 0.f, 1.f); }
    void setFilterMatrix(bool v){ filterMatrix = v; }

    float process(float x) override
    {
        // ---------- LFO ----------
        float mod = filterMatrix ? 0.f : lfo.process();

        // ---------- delay sweep ----------
        float d = baseDelay + depth * mod;

        // ---------- through-zero illusion ----------
        float sign = 1.f;
        if (d < 0.f) {
            d = -d;
            sign = -1.f;
        }

        d = std::max(d, 0.00005f);
        delay.setDelay(d);

        // ---------- read ----------
        float wet = delay.read() * sign;

        // ---------- coloration ----------
        wet = lp.process(wet);
        wet = std::tanh(wet * clipDrive);

        // ---------- feedback ----------
        delay.write(x + wet * feedback);

        // ---------- mix ----------
        return x * (1.f - mix) + wet * mix;
    }

private:
    float sampleRate;

    Delay delay;
    LFO   lfo;
    LowPassFilter lp;

    float baseDelay;
    float depth;
    float feedback;
    float mix;
    float clipDrive;

    bool  filterMatrix = false;
};
