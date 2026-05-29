#pragma once
#include <array>
#include <algorithm>

template <class Sample, int Steps = 16>
class StepSequencer {
public:
    StepSequencer()
    {
        reset();
    }

    void reset()
    {
        mIndex = 0;
        mPhase = (Sample)0;
        mGate = false;
        for (auto& s : mSteps) s = (Sample)0;
    }

    // Set step values (0..1)
    void setStep(int i, Sample v)
    {
        if (i >= 0 && i < Steps)
            mSteps[i] = std::clamp(v, (Sample)0, (Sample)1);
    }

    void setRate(Sample stepsPerBeat)
    {
        mRate = std::max((Sample)0.001, stepsPerBeat);
    }

    // Call per sample
    Sample process(Sample tempoBPM)
    {
        Sample inc = (tempoBPM / (Sample)60) * mRate / (Sample)gam::sampleRate();
        mPhase += inc;

        mGate = false;

        if (mPhase >= (Sample)1)
        {
            mPhase -= (Sample)1;
            mIndex = (mIndex + 1) % Steps;
            mGate = true;
        }

        return mSteps[mIndex];
    }

    bool gate() const { return mGate; }
    int  index() const { return mIndex; }

private:
    std::array<Sample, Steps> mSteps;

    int    mIndex = 0;
    Sample mPhase = (Sample)0;
    Sample mRate  = (Sample)1;
    bool   mGate  = false;
};
