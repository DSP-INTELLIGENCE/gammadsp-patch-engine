class DDXJunoChorusBBD {
public:
    enum Mode { OFF, I, II };

    DDXJunoChorusBBD()
    : left (0.05f, 0.006f),
      right(0.05f, 0.006f)
    {
        left.setFeedback(0.0f);
        right.setFeedback(0.0f);

        lfoL.setWaveform(ModClock::Waveform::Triangle);
        lfoR.setWaveform(ModClock::Waveform::Triangle);

        setMode(I);
    }

    void setMode(Mode m)
    {
        mode = m;

        if (m == I) {
            rate = 0.50f;
            base = 0.0060f;
            depthMs = 0.0015f;
            mix = 0.50f;
        }
        else if (m == II) {
            rate = 0.80f;
            base = 0.0060f;
            depthMs = 0.0025f;
            mix = 0.60f;
        }

        lfoL.setFreq(rate);
        lfoR.setFreq(rate);
    }

    void process(float inL, float inR, float& outL, float& outR)
    {
        if (mode == OFF) {
            outL = inL;
            outR = inR;
            return;
        }

        // slow analog drift
        drift = 0.9997f * drift + 0.0003f * (rand01() - 0.5f);

        float modL = lfoL.process() + drift * 0.15f;
        float modR = lfoR.process() + drift * 0.15f;

        // linear Juno-style delay sweep
        float delayL = base + modL * depthMs + detune;
        float delayR = base + modR * depthMs - detune;

        left.setDelay(delayL);
        right.setDelay(delayR);

        left.setMix(mix);
        right.setMix(mix);

        outL = left.process(inL);
        outR = right.process(inR);
    }

private:
    float rand01() { return float(rand()) / float(RAND_MAX); }

    DDXBBDDelay left, right;
    ModClock lfoL, lfoR;

    Mode mode = OFF;

    float rate = 0.5f;
    float base = 0.006f;
    float depthMs = 0.0015f;
    float mix = 0.5f;

    float detune = 0.0006f;   // stereo offset
    float drift  = 0.f;
};
