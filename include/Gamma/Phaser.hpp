#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Filter.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam 
{
    template<
        class Tv = gam::real,
        class Tp = gam::real,
        class Td = GAM_DEFAULT_DOMAIN
    >
    class Phaser : public Td {
    public:
        /// Default constructor
        Phaser(unsigned stages = 4)
        : mRate(0.5),
        mDepth(1000),
        mCenter(800),
        mMix(0.5),
        mPhase(0)
        {
            setStages(stages);
        }

        // ---- parameters ----

        /// Number of all-pass stages (even numbers typical: 4, 6, 8)
        void setStages(unsigned n){
            mStages.resize(n);
            for(auto& ap : mStages)
                ap.freq(mCenter);
        }

        unsigned stages() const { return mStages.size(); }

        /// LFO rate (Hz)
        void rate(Tp v){ mRate = v; }

        /// Modulation depth (Hz)
        void depth(Tp v){ mDepth = v; }

        /// Center frequency (Hz)
        void center(Tp v){ mCenter = v; }

        /// Dry / wet mix [0,1]
        void mix(Tp v){ mMix = scl::clip(v, Tp(0), Tp(1)); }

        /// Convenience setter
        void set(Tp rate, Tp depth, Tp center, Tp mix){
            mRate   = rate;
            mDepth  = depth;
            mCenter = center;
            mMix    = mix;
        }

        /// Reset filter states and LFO
        void reset(){
            for(auto& ap : mStages)
                ap.reset();
            mPhase = Tp(0);
        }

        // ---- processing ----
        Tv operator()(Tv x){
            // --- LFO ---
            Tp lfo = std::sin(mPhase * Tp(M_2PI));
            mPhase += mRate * Td::ups();
            if(mPhase >= Tp(1)) mPhase -= Tp(1);

            // --- modulated center frequency ---
            Tp f = mCenter + mDepth * lfo;
            f = scl::max(f, Tp(5)); // safety: avoid DC

            // --- cascade all-pass stages ---
            Tv y = x;
            for(auto& ap : mStages){
                ap.freq(f);
                y = ap(y);
            }

            // --- dry/wet mix ---
            return x * (Tp(1) - mMix) + y * mMix;
        }

    private:
        std::vector<AllPass1<Tv, Tp, Td>> mStages;

        Tp mRate;     // LFO rate (Hz)
        Tp mDepth;    // frequency sweep depth (Hz)
        Tp mCenter;   // center frequency (Hz)
        Tp mMix;      // dry/wet
        Tp mPhase;    // LFO phase [0,1)
    };
}