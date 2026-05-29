class HybridReverbStereo {
public:
    HybridReverbStereo()
    : earlyL(0.1f, 4, 0.01f),
      earlyR(0.1f, 4, 0.01f),
      preDiffL1(0.02f), preDiffL2(0.02f),
      preDiffR1(0.02f), preDiffR2(0.02f)
    {
        // pre-diffusers as allpass using Comb trick
        preDiffL1.setDelay(0.003f); preDiffL1.setAllPass(0.7f);
        preDiffL2.setDelay(0.001f); preDiffL2.setAllPass(0.7f);
        preDiffR1.setDelay(0.0032f); preDiffR1.setAllPass(0.7f);
        preDiffR2.setDelay(0.0011f); preDiffR2.setAllPass(0.7f);

        tail.setFeedback(0.78f);
        tail.setDamping(3500.0f);
        tail.setBassCut(120.0f, 0.7f);
        tail.setDrive(1.2f);
        tail.setStereoSpread(1.0f);
        tail.setWidth(1.2f);
    }

    void process(float inL, float inR, float& outL, float& outR) {
        float erL = earlyL.process(inL);
        float erR = earlyR.process(inR);

        float injL = preDiffL2.process(preDiffL1.process(erL));
        float injR = preDiffR2.process(preDiffR1.process(erR));

        float tailL, tailR;
        tail.process(injL, injR, tailL, tailR);

        // Wet/dry (example)
        float wet = 0.6f;
        outL = wet * tailL + (1.0f - wet) * inL;
        outR = wet * tailR + (1.0f - wet) * inR;
    }

private:
    MultitapDelay earlyL, earlyR;
    Comb preDiffL1, preDiffL2, preDiffR1, preDiffR2;
    FDN8Stereo tail;
};
