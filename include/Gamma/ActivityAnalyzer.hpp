#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"
#include "Gamma/Filter.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ActivityAnalyzer : public Td {
public:
    ActivityAnalyzer(){
        rmsLag(0.05f);
        silenceSamples(2000);
        threshold(0.001f);
    }

    /// RMS integration time (seconds)
    void rmsLag(float seconds){
        mRMS.lag(seconds);
    }

    /// Silence threshold (linear RMS)
    void threshold(Tv t){
        mThreshold = scl::max(Tv(1e-6), t);
    }

    /// Number of consecutive samples below threshold to declare silence
    void silenceSamples(unsigned n){
        mSilenceCountMax = n;
    }

    void reset(){
        mRMS.reset();
        mSilenceCount = 0;
        mActive = false;
        mLevel = Tv(0);
    }

    /// Process one sample
    void operator()(Tv x){
        // RMS estimate (squared → smoothed → sqrt)
        Tv e = x * x;
        Tv rms2 = mRMS(e);
        Tv rms  = scl::sqrt(rms2);

        mLevel = rms;

        if(rms < mThreshold){
            if(mSilenceCount < mSilenceCountMax)
                ++mSilenceCount;
        } else {
            mSilenceCount = 0;
        }

        mActive = (mSilenceCount < mSilenceCountMax);
    }

    bool active() const { return mActive; }
    Tv   level()  const { return mLevel; }

protected:
    OnePole<Tv, Tv, Td> mRMS;   // smoothing on squared signal

    unsigned mSilenceCount     = 0;
    unsigned mSilenceCountMax  = 2000;

    Tv   mThreshold = Tv(0.001);
    bool mActive    = false;
    Tv   mLevel     = Tv(0);
};

} // namespace gam
