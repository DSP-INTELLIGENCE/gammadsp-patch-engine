#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/Containers.h"
#include "Gamma/scl.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class RMSFollower : public Td {
public:
    RMSFollower(float windowSeconds = 0.05f){
        window(windowSeconds);
    }

    /// Set RMS window length (seconds)
    /// ⚠️ Call outside audio thread
    void window(float seconds){
        unsigned n = std::max(
            1u,
            unsigned(seconds * Td::spu()) // FIXED
        );

        if(n == mSize) return;

        mBuf.resize(n);
        mBuf.zero();

        mSize  = n;
        mIndex = 0;
        mSumSq = Tv(0);
        mRMS   = Tv(0);
    }

    void reset(){
        mBuf.zero();
        mIndex = 0;
        mSumSq = Tv(0);
        mRMS   = Tv(0);
    }

    /// Per-sample RMS
    Tv operator()(Tv x){
        Tv x2 = x * x;

        mSumSq -= mBuf[mIndex];
        mBuf[mIndex] = x2;
        mSumSq += x2;

        if(++mIndex >= mSize) mIndex = 0;

        mRMS = scl::sqrt(mSumSq / Tv(mSize));
        return mRMS;
    }

    Tv value() const { return mRMS; }
    unsigned size() const { return mSize; }

protected:
    Array<Tv> mBuf;

    unsigned mSize  = 1;
    unsigned mIndex = 0;

    Tv mSumSq = Tv(0);
    Tv mRMS   = Tv(0);
};

} // namespace gam
