#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Filter.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam 
{
    template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
    class PartitionedConvolver: public Td {
    public:
        using Complex = std::complex<Tv>;

        /// blockSize must be power of two
        explicit PartitionedConvolver(unsigned blockSize)
        : mBlockSize(blockSize),
        mFFTSize(blockSize * 2),
        mWritePos(0)
        {
            mInTime.resize(mFFTSize);
            mOutTime.resize(mFFTSize);
            mFreqTmp.resize(mFFTSize);

            mInputHist.resize(mFFTSize, Tv(0));
        }

        /// Set impulse response (offline / non-RT)
        void impulse(const std::vector<Tv>& ir){
            clearIR();

            unsigned numParts =
                (ir.size() + mBlockSize - 1) / mBlockSize;

            mIRParts.resize(numParts);

            for(unsigned p = 0; p < numParts; ++p){
                mIRParts[p].resize(mFFTSize, Complex(0));

                // copy partition
                for(unsigned i = 0; i < mBlockSize; ++i){
                    unsigned idx = p * mBlockSize + i;
                    if(idx < ir.size())
                        mIRParts[p][i] = ir[idx];
                }

                fft(mIRParts[p]);
            }

            mAccum.assign(numParts, std::vector<Complex>(mFFTSize));
            reset();
        }

        /// Clear IR
        void clearIR(){
            mIRParts.clear();
            mAccum.clear();
        }

        /// Reset internal state
        void reset(){
            std::fill(mInputHist.begin(), mInputHist.end(), Tv(0));
            for(auto& a : mAccum)
                std::fill(a.begin(), a.end(), Complex(0));
            mWritePos = 0;
        }

        /// Process one block
        /// input and output are blockSize long
        void process(const Tv* in, Tv* out){
            // ---- shift input history ----
            std::memmove(
                mInputHist.data(),
                mInputHist.data() + mBlockSize,
                sizeof(Tv) * mBlockSize
            );

            std::memcpy(
                mInputHist.data() + mBlockSize,
                in,
                sizeof(Tv) * mBlockSize
            );

            // ---- FFT input ----
            std::fill(mInTime.begin(), mInTime.end(), Complex(0));
            for(unsigned i = 0; i < mFFTSize; ++i)
                mInTime[i] = mInputHist[i];

            fft(mInTime);

            // ---- convolve partitions ----
            for(unsigned p = 0; p < mIRParts.size(); ++p){
                unsigned idx = (mWritePos + p) % mIRParts.size();
                auto& acc = mAccum[idx];

                for(unsigned i = 0; i < mFFTSize; ++i)
                    acc[i] += mInTime[i] * mIRParts[p][i];
            }

            // ---- IFFT output ----
            auto& curr = mAccum[mWritePos];
            ifft(curr);

            for(unsigned i = 0; i < mBlockSize; ++i)
                out[i] = curr[i].real();

            std::fill(curr.begin(), curr.end(), Complex(0));

            mWritePos = (mWritePos + 1) % mIRParts.size();
        }

        unsigned latency() const { return mBlockSize; }

    private:
        unsigned mBlockSize;
        unsigned mFFTSize;
        unsigned mWritePos;

        std::vector<Complex> mInTime;
        std::vector<Complex> mOutTime;
        std::vector<Complex> mFreqTmp;

        std::vector<Tv> mInputHist;
        std::vector<std::vector<Complex>> mIRParts;
        std::vector<std::vector<Complex>> mAccum;

        // ---- FFT stubs (plug Gamma FFT here) ----
        void fft(std::vector<Complex>& v){
            gam::fft(v.data(), v.size());
        }

        void ifft(std::vector<Complex>& v){
            gam::ifft(v.data(), v.size());
        }
    };
}