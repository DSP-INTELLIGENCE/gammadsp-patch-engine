class JCRev {
public:
    JCRev()
    : ap1(0.05f), ap2(0.05f), ap3(0.05f),
      combs{ Comb(0.1f), Comb(0.1f), Comb(0.1f), Comb(0.1f) }
    {
        // Diffusers
        ap1.setDelay(0.0127f); ap1.setAllPass(0.7f);
        ap2.setDelay(0.0093f); ap2.setAllPass(0.7f);
        ap3.setDelay(0.0063f); ap3.setAllPass(0.7f);

        // Comb bank
        const float d[4] = {0.050f, 0.056f, 0.061f, 0.068f};
        const float f[4] = {0.805f, 0.827f, 0.783f, 0.764f};

        for(int i=0;i<4;i++){
            combs[i].setDelay(d[i]);
            combs[i].setFeedback(f[i]);
        }
    }

    float process(float x)
    {
        float y = ap1.process(x);
        y = ap2.process(y);
        y = ap3.process(y);

        float tail = 0.f;
        for(auto& c : combs)
            tail += c.process(y);

        return tail * 0.25f + x * 0.3f;
    }

private:
    Comb ap1, ap2, ap3;
    Comb combs[4];
};

