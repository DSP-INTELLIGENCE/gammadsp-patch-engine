#pragma once
#include <cmath>

class Glide
{
public:
    Glide(double startFreq = 440.0, double timeSeconds = 0.1, double sampleRate = 48000.0)
        : current(startFreq), target(startFreq), sr(sampleRate)
    {
        setTime(timeSeconds);
    }

    void setTime(double seconds)
    {
        time = std::max(1e-6, seconds);
        updateRate();
    }

    void setSampleRate(double sampleRate)
    {
        sr = sampleRate;
        updateRate();
    }

    void setTarget(double freq)
    {
        target = freq;
        updateRate();
    }

    void reset(double freq)
    {
        current = target = freq;
        rate = 0.0;
    }

    inline double process()
    {
        if (current != target)
        {
            const double diff = target - current;

            if (std::abs(diff) <= std::abs(rate))
                current = target;
            else
                current += rate;
        }

        return current;
    }

private:
    void updateRate()
    {
        const double diff = target - current;
        rate = diff / (time * sr);
    }

    double current = 0.0;
    double target = 0.0;
    double rate = 0.0;
    double time = 0.1;
    double sr = 48000.0;
};
