struct MuffToneStack : public ToneStack
{
    TPT1Pole lp;
    TPT1Pole hp;
    Biquad   midCut;

    void prepare(float sr) override
    {
        lp.setCut(8000.f, sr);
        hp.setCut(350.f, sr);

        midCut.setType(gam::PEAKING);
        midCut.setFreq(1000.f);
        midCut.setRes(0.7f);
        midCut.setLevel(-6.0f);   // classic Muff scoop
    }

    void reset() override
    {
        lp.reset();
        hp.reset();
        midCut.reset();
    }

    float process(float x, float tone01) override
    {
        float low  = lp.processLP(x);
        float high = hp.processHP(x);

        float y = (1.f - tone01) * low + tone01 * high;
        return midCut.process(y);
    }
};
