struct KlonToneStack : public ToneStack
{
    TPT1Pole lp;
    TPT1Pole hp;
    Biquad   mid;

    void prepare(float sr) override
    {
        lp.setCut(9000.f, sr);
        hp.setCut(900.f, sr);

        mid.setType(gam::PEAKING);
        mid.setFreq(1000.f);
        mid.setRes(0.7f);
        mid.setLevel(+1.5f);
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
