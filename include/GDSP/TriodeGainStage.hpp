struct TriodeGainStage : public Function
{
    AnalogGainCell cell;
    Triode triode;

    TPT1Pole plateLP;

    void init(float sr)
    {
        cell.init(sr);
        plateLP.setLPCut(8000.f, sr);
        reset();
    }

    void reset()
    {
        cell.reset();
        plateLP.reset();
    }

    float process(float x) override
    {
        float vgk = cell.process(x);
        float vpk = std::max(0.1f, plateLP.s);

        float ip = triode.process(vgk, vpk);
        return plateLP.processLP(-ip);
    }
};
