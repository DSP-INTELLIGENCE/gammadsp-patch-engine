class PRCRev {
public:
    PRCRev()
    : apIn1(0.05f), apIn2(0.05f),
      apOut1(0.05f), apOut2(0.05f),
      comb1(0.1f), comb2(0.1f)
    {
        // Input diffusion
        apIn1.setDelay(0.0077f); apIn1.setAllPass(0.7f);
        apIn2.setDelay(0.0053f); apIn2.setAllPass(0.7f);

        // Comb section (plate resonance)
        comb1.setDelay(0.0297f); comb1.setFeedback(0.805f);
        comb2.setDelay(0.0371f); comb2.setFeedback(0.827f);

        // Output diffusion
        apOut1.setDelay(0.0017f); apOut1.setAllPass(0.7f);
        apOut2.setDelay(0.0009f); apOut2.setAllPass(0.7f);
    }

    float process(float x)
    {
        float y = apIn1.process(x);
        y = apIn2.process(y);

        y = comb1.process(y);
        y = comb2.process(y);

        y = apOut1.process(y);
        y = apOut2.process(y);

        return 0.7f * y + 0.3f * x;
    }

private:
    Comb apIn1, apIn2;
    Comb apOut1, apOut2;
    Comb comb1, comb2;    
};

