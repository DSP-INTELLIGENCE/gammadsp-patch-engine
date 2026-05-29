#pragma once
#include "GDSP_ModDelay.hpp"

class ReverseDelay : public Function
{
public:
    ReverseDelay()
    {
        setTime(0.6f);
        setMix(0.6f);
        setCrossfade(0.02f);
    }

    void setTime(float t)      { time = std::clamp(t, 0.05f, 2.0f); }
    void setMix(float m)       { mix = std::clamp(m, 0.f, 1.f); }
    void setCrossfade(float s) { xfade = std::clamp(s, 0.0f, 0.1f); }

    float process(float input) override
    {
        size_t maxSamp = (size_t)(time * gam::sampleRate());
        if (maxSamp != bufSize)
            resizeBuffers(maxSamp);

        // write into record buffer
        record[recPos++] = input;

        float out = 0.f;

        if (playing)
        {
            out = play[playPos--];

            // crossfade at edges
            float fade = 1.f;
            if (playPos < xfadeSamp) fade = (float)playPos / (float)xfadeSamp;
            if (playPos > bufSize - xfadeSamp)
                fade = (float)(bufSize - playPos) / (float)xfadeSamp;

            out *= std::clamp(fade, 0.f, 1.f);
        }

        // window complete → swap buffers
        if (recPos >= bufSize)
        {
            std::swap(record, play);
            recPos = 0;
            playPos = bufSize - 1;
            playing = true;
        }

        return input * (1.f - mix) + out * mix;
    }

private:
    void resizeBuffers(size_t n)
    {
        bufSize = n;
        record.assign(bufSize, 0.f);
        play.assign(bufSize, 0.f);
        recPos = 0;
        playPos = 0;
        playing = false;
        xfadeSamp = (size_t)(xfade * gam::sampleRate());
    }

    std::vector<float> record, play;

    size_t bufSize = 0;
    size_t recPos = 0;
    size_t playPos = 0;
    size_t xfadeSamp = 0;

    float time = 0.6f;
    float mix = 0.6f;
    float xfade = 0.02f;

    bool playing = false;
};
