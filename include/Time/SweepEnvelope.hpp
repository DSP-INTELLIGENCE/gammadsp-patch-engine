#include "GDSP_Oscillators.hpp"

class SweepEnvelope {
public:

    enum class EnvState {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };
    
    SweepEnvelope()
        : mState(EnvState::Idle),
            mAttack(0.01f), mDecay(0.1f), mSustain(0.7f), mRelease(0.2f),
            mValue(0.0f)
    {}

    void setADSR(float a, float d, float s, float r) {
        mAttack = std::max(a, 0.0001f);
        mDecay  = std::max(d, 0.0001f);
        mSustain = std::clamp(s, 0.0f, 1.0f);
        mRelease = std::max(r, 0.0001f);
    }

    void noteOn() {
        mState = EnvState::Attack;
        mSweep.setPeriod(mAttack);
        mSweep.reset();
    }

    void noteOff() {
        mState = EnvState::Release;
        mSweep.setPeriod(mRelease);
        mSweep.reset();
        mReleaseStart = mValue;
    }

    float process() {
        switch (mState) {

        case EnvState::Idle:
            return 0.0f;

        case EnvState::Attack: {
            float p = mSweep.process();
            mValue = p;                         // 0 → 1
            if (p >= 1.0f) {
                mState = EnvState::Decay;
                mSweep.setPeriod(mDecay);
                mSweep.reset();
            }
            break;
        }

        case EnvState::Decay: {
            float p = mSweep.process();
            mValue = 1.0f + (mSustain - 1.0f) * p;
            if (p >= 1.0f) {
                mState = EnvState::Sustain;
            }
            break;
        }

        case EnvState::Sustain:
            mValue = mSustain;
            break;

        case EnvState::Release: {
            float p = mSweep.process();
            mValue = mReleaseStart * (1.0f - p);
            if (p >= 1.0f) {
                mState = EnvState::Idle;
                mValue = 0.0f;
            }
            break;
        }
        }

        return mValue;
    }

private:
    Sweep    mSweep;
    EnvState mState;

    float mAttack, mDecay, mSustain, mRelease;
    float mValue;
    float mReleaseStart;
};

/*
Usage:
Envelope env;
env.setADSR(0.01f, 0.2f, 0.6f, 0.3f);

env.noteOn();

for (...) {
    float amp = env.process();
    out = osc.process() * amp;
}

env.noteOff();
*/