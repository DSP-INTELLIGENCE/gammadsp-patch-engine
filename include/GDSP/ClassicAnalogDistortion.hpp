class ClassicAnalogDistortion : public DistortionCircuit
{
public:
    void prepare(float sr) override
    {
        sampleRate = sr;
        reset();
    }

    void reset() override
    {
        driveStage = {};
        saturator  = {};
        clipper    = {};
    }

    float process(float x) override
    {
        x = driveStage.process(x);
        x = saturator.process(x);
        x = clipper.process(x);

        return x;
    }

    void setDrive(float v01) override
    {
        driveStage.drive = 0.5f + 8.f * v01;
        saturator.amount = 0.5f + 4.f * v01;
        clipper.drive    = 1.f + 6.f * v01;
    }

    void setTone(float v01) override
    {
        saturator.curve = 0.5f + 2.5f * v01;
        clipper.asym    = 0.3f * v01;
    }

    void setLevel(float v01) override
    {
        driveStage.trim = 0.3f + 1.5f * v01;
        saturator.trim = 0.3f + 1.5f * v01;
        clipper.trim   = 0.3f + 1.5f * v01;
    }

    std::string name() const override { return "Classic Analog"; }

private:
    float sampleRate = 48000.f;

    DriveStage driveStage;
    Saturator  saturator;
    HardClipper clipper;
};
