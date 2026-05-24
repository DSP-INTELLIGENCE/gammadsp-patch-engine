#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Filter.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam 
{

    template<
        unsigned N,
        class Tv = gam::real,
        class Tp = gam::real,
        class Td = GAM_DEFAULT_DOMAIN
    >
    class ModalBank : public Td {
    public:
        using Mode = CombFilter<Tv, Tp, Td>;

        ModalBank(){
            for(unsigned i = 0; i < N; ++i){
                mGain[i] = Tp(1) / Tp(N);
            }
        }

        /// Reset all modes
        void reset(){
            for(auto& m : mModes)
                m.reset();
        }

        /// Set global excitation scaling
        void gain(Tp g){ mGlobalGain = g; }

        /// Access a mode directly
        Mode& mode(unsigned i){ return mModes[i]; }
        const Mode& mode(unsigned i) const { return mModes[i]; }

        /// Set mode frequency (Hz)
        void freq(unsigned i, Tp hz){
            mModes[i].freq(hz);
        }

        /// Set mode resonance [0,1]
        void res(unsigned i, Tp r){
            mModes[i].res(r);
        }

        /// Set mode damping (physical mode)
        void damp(unsigned i, Tp d){
            mModes[i].damp(d);
        }

        /// Set per-mode output gain
        void gain(unsigned i, Tp g){
            mGain[i] = g;
        }

        /// Set all modes at once (convenience)
        void setModes(const Tp* freqs, const Tp* gains, const Tp* res = nullptr){
            for(unsigned i = 0; i < N; ++i){
                mModes[i].freq(freqs[i]);
                if(res)   mModes[i].res(res[i]);
                if(gains) mGain[i] = gains[i];
            }
        }

        /// Process one sample
        Tv operator()(Tv in){
            Tv sum = Tv(0);
            for(unsigned i = 0; i < N; ++i){
                sum += mModes[i](in) * mGain[i];
            }
            return sum * mGlobalGain;
        }

        static constexpr unsigned size(){ return N; }

    private:
        Mode mModes[N];
        Tp   mGain[N];
        Tp   mGlobalGain = Tp(1);
    };

} // namespace gam
