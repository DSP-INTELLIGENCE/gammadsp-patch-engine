#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Filter.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam 
{
    template<
        unsigned N = 4,
        class Tv = gam::real,
        template<class> class Si = ipl::Linear,
        class Tp = gam::real,
        class Td = GAM_DEFAULT_DOMAIN
    >
    class FDNReverb:: public Td {
    public:
        static_assert((N & (N - 1)) == 0, "N must be power of two");

        using DelayT = Delay<Tv, Si, Td>;

        FDNReverb(){
            for(unsigned i = 0; i < N; ++i){
                mGain[i] = Tp(0.7);
                mLP[i].freq(6000);
            }
        }

        /// Set delays (seconds)
        void delays(const std::array<float, N>& d){
            for(unsigned i = 0; i < N; ++i){
                mDelay[i].maxDelay(d[i]);
                mDelay[i].delay(d[i]);
            }
        }

        /// Set global decay (RT60-like)
        void decay(Tp v){
            for(unsigned i = 0; i < N; ++i)
                mGain[i] = scl::clip(v, Tp(0), Tp(0.999));
        }

        /// Set damping (HF loss)
        void damp(Tp v){
            for(unsigned i = 0; i < N; ++i)
                mLP[i].freq(200.0 + (1.0 - v) * 8000.0);
        }

        /// Reset state
        void reset(){
            for(auto& d : mDelay)
                if(d.isSoleOwner()) d.zero();
            for(auto& f : mLP)
                f.reset();
        }

        /// Process one sample
        Tv operator()(Tv in){
            // read delay outputs
            for(unsigned i = 0; i < N; ++i)
                mOut[i] = mDelay[i].read();

            // Hadamard mix
            hadamard(mOut);

            // feedback + filtering
            for(unsigned i = 0; i < N; ++i){
                Tv v = mLP[i](mOut[i]) * mGain[i];
                mDelay[i].write(v + in * Tp(0.25));
            }

            // sum outputs
            Tv sum = Tv(0);
            for(auto v : mOut) sum += v;
            return sum * Tp(1.0 / N);
        }

    private:
        DelayT mDelay[N];
        OnePole<Tv, Tp, Td> mLP[N];
        Tp mGain[N];
        Tv mOut[N];

        /// In-place Hadamard transform
        void hadamard(Tv* x){
            for(unsigned h = 1; h < N; h <<= 1){
                for(unsigned i = 0; i < N; i += h << 1){
                    for(unsigned j = i; j < i + h; ++j){
                        Tv a = x[j];
                        Tv b = x[j + h];
                        x[j]       = a + b;
                        x[j + h]   = a - b;
                    }
                }
            }
        }
    };
}