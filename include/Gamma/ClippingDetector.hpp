#pragma once
#include <cmath>
#include <algorithm>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ClippingDetector : public Td {
public:
    ClippingDetector(){
        reset();
    }

    /// Set clip threshold (linear, default = 1.0)
    void threshold(Tv t){
        mThresh = std::max(t, Tv(0));
    }

    /// Latching mode:
    /// true  = once clipped, stays clipped until reset()
    /// false = reflects clipping on current sample only
    void latch(bool v){
        mLatch = v;
    }

    void reset(){
        mClipped = false;
    }

    /// Process sample (passes signal through unchanged)
    inline Tv operator()(Tv x){
        const bool hit = std::fabs(x) >= mThresh;

        if(mLatch){
            mClipped |= hit;
        } else {
            mClipped = hit;
        }

        return x;
    }

    /// Query state
    bool clipped() const { return mClipped; }

    void onDomainChange(double){
        // nothing sample-rate dependent here,
        // but kept for Gamma consistency
    }

private:
    Tv   mThresh  { Tv(1) };
    bool mLatch   { true };
    bool mClipped { false };
};

} // namespace gam
