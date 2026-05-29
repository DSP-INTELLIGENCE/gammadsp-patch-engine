#pragma once
#include "GDSP_ModDelay.hpp"

class Looper : public Function
{
public:
    Looper()
    {
        setMaxTime(10.0f);
        setMix(1.0f);
        setFeedback(0.95f);
    }

    void setMaxTime(float sec)
    {
        maxSamples = (size_t)(sec * gam::sampleRate());
        buffer.assign(maxSamples, 0.f);
    }

    void setMix(float m)       { mix = std::clamp(m, 0.f, 1.f); }
    void setFeedback(float f)  { feedback = std::clamp(f, 0.f, 0.999f); }

    void record(bool r)        { recording = r; }
    void overdub(bool o)       { overdubbing = o; }
    void play(bool p)          { playing = p; }

    void setReverse(bool r)    { reverse = r; }
    void setHalfSpeed(bool h)  { halfSpeed = h; }
    void setFreeze(bool f)     { freeze = f; }

    float process(float input) override
    {
        if (!playing && !recording) return input;

        float out = buffer[playPos];

        if (recording)
        {
            buffer[writePos] = input;
            loopLen = writePos + 1;
        }
        else if (overdubbing && !freeze)
        {
            buffer[writePos] = buffer[writePos] * feedback + input;
        }

        // advance pointers
        if (!freeze)
        {
            if (!reverse)
            {
                writePos = (writePos + 1) % maxSamples;
                playPos  = (playPos  + (halfSpeed ? 0.5f : 1.f)) ;
            }
            else
            {
                playPos  = (playPos  + (halfSpeed ? -0.5f : -1.f));
            }

            if (playPos >= loopLen) playPos = 0;
            if (playPos < 0)        playPos = loopLen - 1;
        }

        return input * (1.f - mix) + out * mix;
    }

private:
    std::vector<float> buffer;

    size_t maxSamples = 0;
    size_t loopLen = 0;

    size_t writePos = 0;
    float  playPos  = 0;

    float mix = 1.f;
    float feedback = 0.95f;

    bool recording  = false;
    bool overdubbing = false;
    bool playing = false;

    bool reverse = false;
    bool halfSpeed = false;
    bool freeze = false;
};
