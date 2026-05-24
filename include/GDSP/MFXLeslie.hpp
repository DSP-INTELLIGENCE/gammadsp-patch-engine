struct MFXRotor
{
    ModDelay delay;
    Modulator lfo;
    Modulator depth;

    float speed = 0.3f;
    float targetSpeed = 0.3f;

    float accel = 0.15f;   // inertia

    float process(float x)
    {
        speed += (targetSpeed - speed) * accel;

        lfo.set(speed);

        float mod = lfo.process() * depth.process();
        delay.setDM(mod);

        return delay.update(x);
    }
};

class MFXLeslieSim
{
public:
    MFXLeslieSim()
    {
        // Drum (slow, low)
        drum.delay.setDelay(0.006f);
        drum.depth.set(0.0012f);

        // Horn (fast, bright)
        horn.delay.setDelay(0.002f);
        horn.depth.set(0.0006f);

        setSlow();
    }

    void setSlow()
    {
        drum.targetSpeed = 0.4f;
        horn.targetSpeed = 0.8f;
    }

    void setFast()
    {
        drum.targetSpeed = 0.8f;
        horn.targetSpeed = 5.5f;
    }

    float process(float x, float& outL, float& outR)
    {
        float low = lowpass.process(x);
        float high = x - low;

        float d = drum.process(low);
        float h = horn.process(high);

        float mix = d + h;

        // Stereo rotation
        float phase = horn.lfo.last();   // reuse LFO phase
        outL = mix * (0.7f + 0.3f * std::sin(phase));
        outR = mix * (0.7f + 0.3f * std::sin(phase + M_PI));

        return 0.5f * (outL + outR);
    }

private:
    MFXRotor drum, horn;
    OnePoleLPF lowpass;
};
