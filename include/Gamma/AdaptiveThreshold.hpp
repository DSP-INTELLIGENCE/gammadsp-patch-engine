#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class AdaptiveThreshold : public Td {
public:
    AdaptiveThreshold(
        T meanTime = T(0.5),      // seconds
        T devTime  = T(0.05),     // seconds
        T sens     = T(1.5)
    ){
        meanTimeConstant(meanTime);
        deviationTimeConstant(devTime);
        sensitivity(sens);
        reset();
    }

    void reset(){
        mMean = T(0);
        mDev  = T(0);
        mThresh = T(0);
    }

    /// Time constant for mean adaptation (seconds)
    void meanTimeConstant(T sec){
        mMeanTime = scl::max(sec, T(1e-6));
        updateMeanCoeff();
    }

    /// Time constant for deviation adaptation (seconds)
    void deviationTimeConstant(T sec){
        mDevTime = scl::max(sec, T(1e-6));
        updateDevCoeff();
    }

    /// How far above mean the threshold sits
    void sensitivity(T s){
        mSens = scl::max(s, T(0));
    }

    /// Per-sample update
    T operator()(T x){
        // Mean
        T d = x - mMean;
        mMean += mAlphaMean * d;

        // Mean absolute deviation (robust)
        T ad = scl::abs(d);
        mDev += mAlphaDev * (ad - mDev);

        // Threshold
        mThresh = mMean + mSens * mDev;
        return mThresh;
    }

    T value()     const { return mThresh; }
    T mean()      const { return mMean; }
    T deviation() const { return mDev; }

    void onDomainChange(double){
        updateMeanCoeff();
        updateDevCoeff();
    }

private:
    void updateMeanCoeff(){
        mAlphaMean = T(1) - scl::exp(T(-1) / (mMeanTime * Td::spu()));
    }

    void updateDevCoeff(){
        mAlphaDev = T(1) - scl::exp(T(-1) / (mDevTime * Td::spu()));
    }

    T mMean   = T(0);
    T mDev    = T(0);
    T mThresh = T(0);

    T mSens = T(1.5);

    T mMeanTime = T(0.5);
    T mDevTime  = T(0.05);

    T mAlphaMean = T(0.01);
    T mAlphaDev  = T(0.05);
};

} // namespace gam
