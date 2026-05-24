#pragma once
#include <algorithm>
#include <cmath>

#include "OnsetDetector.hpp"
#include "PCounter.hpp"
#include "EnergyTracker.hpp"

template <class Sample>
class BeatTracker {
public:
    struct State {
        Sample tempoBPM = (Sample)0;
        Sample confidence = (Sample)0;
        bool   beat = false;
        bool   downbeat = false;
    };

    BeatTracker()
    : mOnset(),
      mClock(),
      mEnergy(),
      mState()
    {
        reset();
    }

    void reset()
    {
        mClock.reset();
        mEnergy.reset();
        mOnset.reset();
        mState = {};
        mPhase = (Sample)0;
    }

    // audio thread
    void processSample(Sample x)
    {
        mEnergy.process(x);
        mLastOnset = mOnset.process(x);
    }

    // control thread
    void update()
    {
        if (mLastOnset)
        {
            bool tick = mClock.process(true);
            if (tick && mClock.locked())
            {
                mTempo = mClock.tempo();
                mState.tempoBPM = mTempo;
                mState.confidence = (Sample)1;
            }
        }
        else
        {
            mClock.process(false);
        }

        advancePhase();
    }

    const State& state() const { return mState; }

private:
    void advancePhase()
    {
        if (mState.tempoBPM <= (Sample)0) return;

        Sample beatHz = mState.tempoBPM / (Sample)60;
        Sample inc = beatHz / (Sample)gam::sampleRate();

        mPhase += inc;

        mState.beat = false;
        mState.downbeat = false;

        if (mPhase >= (Sample)1)
        {
            mPhase -= (Sample)1;
            mState.beat = true;

            if (++mBeatCount % 4 == 0)
                mState.downbeat = true;
        }
    }

private:
    OnsetDetector<Sample>  mOnset;
    PCounter<Sample>      mClock;
    EnergyTracker<Sample> mEnergy;

    bool   mLastOnset = false;
    Sample mTempo = (Sample)0;
    Sample mPhase = (Sample)0;
    unsigned mBeatCount = 0;

    State mState;
};
