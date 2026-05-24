#pragma once

class PowerSag
{
public:
    PowerSag()
    : env(4.0f) {}   // very slow response by default

    void prepare()
    {
        env.reset();
    }

    void setAmount(float a)
    {
        amount = std::clamp(a, 0.f, 1.f);
    }

    void setRecovery(float seconds)
    {
        env.setLag(seconds);
    }

    inline void apply(float input,
                      float& drive,
                      float& outputGain)
    {
        float e = env.process(std::fabs(input));

        // Sag reduces available headroom & drive
        float sag = 1.f - e * 0.6f * amount;

        drive *= sag;

        // Compensate loudness slightly (feels like tube recovery)
        outputGain *= (1.f + (1.f - sag) * 0.4f);
    }

private:
    float amount = 0.f;
    EnvFollow env;
};
