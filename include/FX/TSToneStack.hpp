struct TSToneStack : public ToneStack
{
    TPT1Pole lp;
    TPT1Pole hp;
    Biquad   mid;

    void prepare(float sr) override
    {
        lp.setCut(1800.f, sr);
        hp.setCut(720.f, sr);

        mid.setType(gam::PEAKING);
        mid.setFreq(720.f);
        mid.setRes(0.8f);
        mid.setLevel(+3.5f);   // dB mid hump
    }

    void reset() override
    {
        lp.reset();
        hp.reset();
        mid.reset();
    }

    float process(float x, float tone01) override
    {
        float dark   = lp.processLP(x);
        float bright = hp.processHP(x);

        float y = (1.f - tone01) * dark + tone01 * bright;
        return mid.process(y);
    }
};
