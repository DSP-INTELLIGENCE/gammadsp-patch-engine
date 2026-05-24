#pragma once
#include "BandControls.hpp"

class LR4Crossover3Band
{
public:
    LR4Crossover3Band(float sr=48000.0f)
    : router(sr)
    {
        setSampleRate(sr);
        setFrequencies(200.0f, 2000.0f);
        router.updateTargets();
    }

    void setSampleRate(float sr){
        sampleRate = sr;
        router.setSampleRate(sr);
    }

    void setFrequencies(float lowMidHz, float midHighHz)
    {
        f1 = lowMidHz;
        f2 = midHighHz;
        x1.setCutoff(f1);
        x2.setCutoff(f2);
    }

    void reset(){
        x1.reset();
        x2.reset();
        router.updateTargets();
        router.forceReset();
    }


    // Expose controls
    BandControls& low()  { return router.c[0]; }
    BandControls& mid()  { return router.c[1]; }
    BandControls& high() { return router.c[2]; }

    void setMakeup(bool v){
        router.enableMakeup = v;
        router.updateTargets();
    }

    // Call this after UI changes: mute/solo/gainDb
    void onParamsChanged(){
        router.updateTargets();
    }

    float process(float in, float* outLow=nullptr, float* outMid=nullptr, float* outHigh=nullptr)
    {
        float lowBand, hiAfterLow;
        x1.process(in, lowBand, hiAfterLow);

        float midBand, highBand;
        x2.process(hiAfterLow, midBand, highBand);

        return router.process(lowBand, midBand, highBand, outLow, outMid, outHigh);
    }

private:
    float sampleRate = 48000.0f;
    float f1 = 200.0f, f2 = 2000.0f;

    LR4Crossover x1; // low/mid split
    LR4Crossover x2; // mid/high split

    BandRouter router;
};
