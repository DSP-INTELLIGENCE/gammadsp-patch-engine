class FDNReverb4 : public Function {
public:
    FDNReverb4()
    : apIn1(0.05f), apIn2(0.05f),
      apOut1(0.05f), apOut2(0.05f)
    {
        // Input diffusion
        apIn1.setDelay(0.0048f); apIn1.setAllPass(0.75f);
        apIn2.setDelay(0.0035f); apIn2.setAllPass(0.75f);

        // Output diffusion
        apOut1.setDelay(0.0063f); apOut1.setAllPass(0.70f);
        apOut2.setDelay(0.0041f); apOut2.setAllPass(0.70f);

        float times[4] = { 0.0313f, 0.0371f, 0.0411f, 0.0531f };
        for(int i=0;i<4;i++)
        {
            delays[i].setDelay(times[i]);
            damp[i].setFreq(6500.0f);
        }

        setDecay(1.6f);
        setMix(0.35f);
    }

    void setDecay(float rt60)
    {
        decay = std::clamp(rt60, 0.2f, 8.0f);
        updateFeedback();
    }

    void setDamping(float hz)
    {
        for(auto& d : damp)
            d.setFreq(hz);
    }

    void setMix(float m)
    {
        mix = std::clamp(m, 0.0f, 1.0f);
    }

    void setSize(float s)
    {
        size = std::clamp(s, 0.3f, 2.0f);
        float base[4] = {0.0313f, 0.0371f, 0.0411f, 0.0531f};
        for(int i=0;i<4;i++)
            delays[i].setDelay(base[i] * size);
        updateFeedback();
    }

    float process(float x) override
    {
        // Input diffusion
        x = apIn2.process(apIn1.process(x));

        // Read
        float d[4];
        for(int i=0;i<4;i++) d[i] = delays[i].read();

        // 4x4 Hadamard
        float y[4] = {
            0.5f*( d[0]+d[1]+d[2]+d[3]),
            0.5f*( d[0]-d[1]+d[2]-d[3]),
            0.5f*( d[0]+d[1]-d[2]-d[3]),
            0.5f*( d[0]-d[1]-d[2]+d[3])
        };

        // Feedback + damping
        for(int i=0;i<4;i++)
            y[i] = damp[i].process(y[i]) * fb[i];

        // Write
        for(int i=0;i<4;i++)
            delays[i].write(x + y[i]);

        // Output taps
        float L = 0.5f*( d[0] + d[1] - d[2] - d[3] );
        float R = 0.5f*( d[0] - d[1] + d[2] - d[3] );

        // Output diffusion
        L = apOut2.process(apOut1.process(L));
        R = apOut2.process(apOut1.process(R));

        return (x*(1.f-mix) + 0.5f*(L+R)*mix);
    }

private:
    void updateFeedback()
    {
        for(int i=0;i<4;i++)
        {
            float t = delays[i].getDelayTime();
            fb[i] = powf(10.f, -3.f * t / decay);
        }
    }

private:
    Comb apIn1, apIn2;
    Comb apOut1, apOut2;

    Delay delays[4];
    OnePole damp[4];

    float fb[4];

    float mix  = 0.35f;
    float size = 1.0f;
    float decay = 1.6f;
};

