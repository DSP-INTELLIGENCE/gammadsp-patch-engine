#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Filter.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam 
{
    template<class Tv = gam::real,
            class Tp = gam::real,
            class Td = GAM_DEFAULT_DOMAIN>
    class CombFilter : public Td {
    public:
        enum Mode {
            FEEDFORWARD,
            FEEDBACK,
            ALLPASS,
            PHYSICAL
        };

        /// Default constructor (no allocation)
        CombFilter()
        : mMode(FEEDBACK)
        {
            onDomainChange(1);
            setRes(0.25f);
            setFreq(440);
        }

        /// Allocate with maximum delay (seconds)
        explicit CombFilter(float maxDelaySec)
        : mComb(maxDelaySec, 0.01f)
        {
            mMaxDelay = maxDelaySec;
            onDomainChange(1);
        }

        /// Set operating mode
        void mode(Mode m){
            mMode = m;
            mFbMax = mFfMax = mApMax = Tp(0);

            if(m == FEEDBACK || m == PHYSICAL) mFbMax = Tp(0.995);
            if(m == FEEDFORWARD)               mFfMax = Tp(0.95);
            if(m == ALLPASS)                   mApMax = Tp(0.95);
        }

        Mode mode() const { return mMode; }

        /// Set fundamental frequency (Hz)
        void freq(Tp hz){
            mFreq = scl::max(hz, Tp(1e-6));
            updateDelay();
        }

        Tp freq() const { return mFreq; }

        /// Set resonance [0,1]
        void res(Tp r){
            mRes = scl::clip(r, Tp(0), Tp(1));
            updateGains();
        }

        Tp res() const { return mRes; }

        /// Damping for PHYSICAL mode [0,1]
        void damp(Tp d){
            mDamp = scl::clip(d, Tp(0), Tp(1));
        }

        /// Invert feedback polarity
        void invertFeedback(bool v){ mInvertFb = v; }

        /// Reset internal state
        void reset(){
            if(mComb.isSoleOwner())
                mComb.zero();
            mDampZ = Tv(0);
        }

        /// Process one sample
        Tv operator()(Tv in){
            applyParams();

            // --- physical / waveguide mode ---
            if(mMode == PHYSICAL){
                Tv d = mComb.read();

                // loss filter in feedback loop
                Tp a = Tp(0.001) + (Tp(1) - mDamp) * Tp(0.2);
                mDampZ += a * (d - mDampZ);

                Tp fb = mInvertFb ? -mFb : mFb;
                return mComb.circulateFbk(in, mDampZ * fb);
            }

            // --- standard comb paths ---
            return mComb(in);
        }

        void onDomainChange(double /*r*/){
            updateDelay();
        }

    private:
        // ---- internal helpers ----

        void updateDelay(){
            Tp d = Tp(1) / mFreq;
            d = scl::clip(d, mMinDelay, mMaxDelay);
            mDelay = d;
        }

        void updateGains(){
            constexpr Tp k = Tp(6);
            Tp shaped = Tp(1) - std::exp(-k * mRes);

            mFb = shaped * mFbMax;
            mFf = shaped * mFfMax;
            mAp = shaped * mApMax;

            mFb = scl::clip(mFb, Tp(-0.999), Tp(0.999));
            mFf = scl::clip(mFf, Tp(-0.999), Tp(0.999));
            mAp = scl::clip(mAp, Tp(-0.999), Tp(0.999));
        }

        void applyParams(){
            mComb.delay(mDelay);

            if(mMode == ALLPASS){
                mComb.allPass(mAp);
            }
            else {
                mComb.fbk(mFb);
                mComb.ffd(mFf);
            }
        }

    private:
        Comb<Tv, ipl::Linear, Tp, Td> mComb;

        Mode mMode;

        Tp mFreq = Tp(440);
        Tp mRes  = Tp(0.25);
        Tp mDamp = Tp(0.5);

        Tp mDelay = Tp(0.01);

        Tp mFb = Tp(0);
        Tp mFf = Tp(0);
        Tp mAp = Tp(0);

        Tp mFbMax = Tp(0);
        Tp mFfMax = Tp(0);
        Tp mApMax = Tp(0);

        Tp mMaxDelay = Tp(2);
        Tp mMinDelay = Tp(1) / Tp(8000);

        bool mInvertFb = false;
        Tv   mDampZ = Tv(0);
    };
} // namespace gam
