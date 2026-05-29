class HybridReverb {
public:
    HybridReverb()
    : early(0.1f, 4, 0.01f),
      preDiff1(0.01f), preDiff2(0.01f) // use Comb as allpass diffusers like you did
    {
        early.setIpolType(LINEAR);

        preDiff1.setDelay(0.003f); preDiff1.setAllPass(0.7f);
        preDiff2.setDelay(0.001f); preDiff2.setAllPass(0.7f);

        tail.setFeedback(0.78f);
        tail.setDamping(3500.0f);   // HF RT60 shorter
        tail.setBassCut(120.0f, 0.7f);
        tail.setDrive(1.2f);
    }

    float process(float x) {
        float er = early.process(x);

        // pre-diffuse the injection (smooths onset, increases density)
        float inj = preDiff2.process(preDiff1.process(er));

        float late = tail.process(inj);

        // post diffusion (optional) can be added with AllPass1/2 or your Comb-allpass trick
        return 0.6f * late + 0.4f * x;
    }

private:
    MultitapDelay early;
    Comb preDiff1, preDiff2; // acting as allpass diffusers
    FDN4 tail;
};
