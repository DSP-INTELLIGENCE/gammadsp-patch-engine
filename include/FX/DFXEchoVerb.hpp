#pragma once
#include "DFXDigitalDelay.hpp"
#include "RFXFreeverb.hpp"

class EchoVerb {
public:
    EchoVerb()
    : mDelay(2.0f, 0.35f)
    {
        mDelay.setFeedback(0.45f);
        mDelay.setMix(1.0f);     // full wet from delay into reverb
        mReverb.setRoomSize(0.7f);
    }

    void setDelayTime(float sec)  { mDelay.setDelay(sec); }
    void setDelayFeedback(float f){ mDelay.setFeedback(f); }
    void setDelayMix(float m)     { mDelay.setMix(m); }
    void setReverbSize(float s)   { mReverb.setRoomSize(s); }
    void setWet(float w)          { mWet = std::clamp(w, 0.f, 1.f); }

    float process(float x)
    {
        float echo = mDelay.process(x);
        float rev  = mReverb.process(echo);
        return x * (1.f - mWet) + rev * mWet;
    }

private:
    DFXDigitalDelay mDelay;
    RFXFreeVerb     mReverb;
    float        mWet = 0.4f;
};


