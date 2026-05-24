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
        template<class> class Si = ipl::Linear,
        class Td = GAM_DEFAULT_DOMAIN
    >
    class TapMatrixDelay : public Delay<Tv, Si, Td> {
    public:
        using Base = Delay<Tv, Si, Td>;

        /// One tap
        struct Tap {
            uint32_t delay; // fixed-point delay
            bool active;
        };

        /// Constructor
        TapMatrixDelay() {}

        /// Allocate buffer, taps, and outputs
        TapMatrixDelay(float maxDelay,
                    unsigned numTaps,
                    unsigned numOuts)
        {
            Base::maxDelay(maxDelay);
            taps(numTaps);
            outputs(numOuts);
        }

        // ---- taps ----
        void taps(unsigned n){
            mTaps.resize(n);
            for(auto& t : mTaps){
                t.delay = 0;
                t.active = true;
            }
            resizeMatrix();
        }

        unsigned taps() const { return mTaps.size(); }

        void delay(float v, unsigned tap){
            mTaps[tap].delay = this->delayFToI(v);
        }

        void delaySamplesR(float samples, unsigned tap){
            mTaps[tap].delay = this->delayFToI(samples * Td::ups());
        }

        // ---- outputs ----
        void outputs(unsigned n){
            mOuts.resize(n);
            resizeMatrix();
        }

        unsigned outputs() const { return mOuts.size(); }

        // ---- matrix ----
        /// Set matrix coefficient (out, tap)
        void coef(unsigned out, unsigned tap, Tv v){
            mMatrix[out * taps() + tap] = v;
        }

        /// Get output sample
        const Tv& out(unsigned i) const { return mOuts[i]; }

        // ---- processing ----
        void operator()(const Tv& x){
            // --- READ TAPS ---
            mTapBuf.resize(mTaps.size());
            for(unsigned i=0;i<mTaps.size();++i){
                const auto& t = mTaps[i];
                mTapBuf[i] = t.active
                    ? this->mIpol(*this, this->mPhase - t.delay)
                    : Tv(0);
            }

            // --- MIX MATRIX ---
            for(unsigned o=0;o<mOuts.size();++o){
                Tv sum(0);
                Tv* row = &mMatrix[o * mTaps.size()];
                for(unsigned i=0;i<mTaps.size();++i){
                    sum += row[i] * mTapBuf[i];
                }
                mOuts[o] = sum;
            }

            // --- WRITE INPUT ---
            Base::write(x);
        }

    private:
        std::vector<Tap> mTaps;
        std::vector<Tv>  mMatrix;   // row-major [out][tap]
        std::vector<Tv>  mTapBuf;   // tap read buffer
        std::vector<Tv>  mOuts;

        void resizeMatrix(){
            mMatrix.resize(mOuts.size() * mTaps.size(), Tv(0));
        }
    };
}