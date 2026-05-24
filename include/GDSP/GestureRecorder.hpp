#pragma once
#include <array>
#include <algorithm>

template <class Sample, int MaxSamples = 48000>
class GestureRecorder {
public:
    GestureRecorder()
    {
        reset();
    }

    void reset()
    {
        mWrite = 0;
        mRead = 0;
        mLength = 0;
        mRecording = false;
        mPlaying = false;
    }

    void startRecord()
    {
        reset();
        mRecording = true;
    }

    void stopRecord()
    {
        mRecording = false;
        mLength = mWrite;
        mPlaying = true;
        mRead = 0;
    }

    void startPlay()
    {
        if (mLength > 0)
        {
            mPlaying = true;
            mRead = 0;
        }
    }

    void stopPlay()
    {
        mPlaying = false;
    }

    // per sample
    Sample process(Sample input)
    {
        if (mRecording && mWrite < MaxSamples)
        {
            mBuffer[mWrite++] = input;
        }

        if (mPlaying && mLength > 0)
        {
            Sample out = mBuffer[mRead++];
            if (mRead >= mLength)
                mRead = 0;
            return out;
        }

        return input;
    }

private:
    std::array<Sample, MaxSamples> mBuffer;

    int  mWrite = 0;
    int  mRead = 0;
    int  mLength = 0;

    bool mRecording = false;
    bool mPlaying = false;
};
