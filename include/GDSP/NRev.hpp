class NRev {
public:
    NRev()
    : apIn1(0.05f), apIn2(0.05f),
      apOut1(0.05f), apOut2(0.05f),
      combs{ Comb(0.2f), Comb(0.2f), Comb(0.2f), Comb(0.2f) }
    {
        // Input diffusion
        apIn1.setDelay(0.0127f); apIn1.setAllPass(0.7f);
        apIn2.setDelay(0.0093f); apIn2.setAllPass(0.7f);

        // Comb bank
        const float d[4] = {0.050f, 0.056f, 0.061f, 0.068f};
        const float f[4] = {0.805f, 0.827f, 0.783f, 0.764f};
        for(int i=0;i<4;i++){
            combs[i].setDelay(d[i]);
            combs[i].setFeedback(f[i]);
        }

        // Output diffusion
        apOut1.setDelay(0.0063f); apOut1.setAllPass(0.7f);
        apOut2.setDelay(0.0041f); apOut2.setAllPass(0.7f);
    }

    float process(float x)
    {
        float y = apIn1.process(x);
        y = apIn2.process(y);

        float t = 0.f;
        for(auto& c : combs)
            t += c.process(y);
        t *= 0.25f;

        t = apOut1.process(t);
        t = apOut2.process(t);

        return t * 0.7f + x * 0.3f;
    }

private:
    Comb apIn1, apIn2;
    Comb apOut1, apOut2;
    Comb combs[4];
};

