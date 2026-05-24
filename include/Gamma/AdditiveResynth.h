#pragma once
#include "Gamma/Oscillator.h"
#include "Gamma/Filter.h"
#include "Gamma/Containers.h"
#include "Gamma/Domain.h"

namespace gam {

template <class T = real, class Td = GAM_DEFAULT_DOMAIN>
class AdditiveResynth : public Td {
public:
    /// \param[in] fftSize  FFT size used in analysis
    /// \param[in] hopSize  Hop size (unused here but kept for symmetry)
    AdditiveResynth(unsigned fftSize, unsigned hopSize = 0)
    :   mFFTSize(fftSize),
        mHopSize(hopSize)
    {
        resizeBins(fftSize/2 + 1);
    }

    // ------------------------------------------------------------
    // Controls
    // ------------------------------------------------------------

    AdditiveResynth& ampScale(T v)      { mAmpScale = v; return *this; }
    AdditiveResynth& magThreshold(T v)  { mMagThresh = scl::max(v, T(0)); return *this; }

    /// Smoothing coefficient in units (seconds)
    AdditiveResynth& smoothing(T seconds){
        mSmoothTime = seconds;
        for(auto& s : mAmpSmooth) s.lag(seconds);
        return *this;
    }

    /// Reset all oscillator phases (e.g., on transient)
    void resetPhase(){
        for(auto& o : mOsc) o.phase(T(0));
    }

    // ------------------------------------------------------------
    // Analysis → oscillator update (call once per STFT frame)
    // ------------------------------------------------------------

    /// magFreqInterleaved = { mag0, freq0, mag1, freq1, ... }
    void analyzeFrame(const T* magFreqInterleaved){
        const unsigned bins = mOsc.size();

        for(unsigned k = 1; k + 1 < bins; ++k){
            const T mag  = magFreqInterleaved[2*k + 0];
            const T freq = magFreqInterleaved[2*k + 1];

            if(mag < mMagThresh || freq <= T(0)){
                mAmpSmooth[k] = T(0);
                mOsc[k].amp(T(0));
                continue;
            }

            mOsc[k].freq(freq);

            T targetAmp = mag * mAmpScale;
            T smoothed  = mAmpSmooth[k](targetAmp);
            mOsc[k].amp(smoothed);
        }
    }

    template <class STFTType>
    void analyze(const STFTType& stft){
        const unsigned bins = mOsc.size();
        const auto* sp = stft.bins();

        for(unsigned k = 1; k + 1 < bins; ++k){
            T mag  = sp[k].r;
            T freq = sp[k].i;

            if(mag < mMagThresh || freq <= T(0)){
                mAmpSmooth[k] = T(0);
                mOsc[k].amp(T(0));
                continue;
            }

            mOsc[k].freq(freq);
            T target = mag * mAmpScale;
            mOsc[k].amp(mAmpSmooth[k](target));
        }
    }

    // ------------------------------------------------------------
    // Synthesis
    // ------------------------------------------------------------

    /// Generate one sample
    T operator()(){
        T y(0);
        for(unsigned k = 1; k + 1 < mOsc.size(); ++k){
            y += mOsc[k]().real();
        }
        return y;
    }

    /// Generate block
    void operator()(T* out, unsigned n){
        for(unsigned i=0; i<n; ++i) out[i] = (*this)();
    }

    // ------------------------------------------------------------
    // Utilities
    // ------------------------------------------------------------

    unsigned numBins() const { return mOsc.size(); }

    void resizeBins(unsigned bins){
        mOsc.resize(bins);
        mAmpSmooth.resize(bins);

        for(auto& s : mAmpSmooth){
            s.type(SMOOTHING);
            s.lag(mSmoothTime);
        }
    }

protected:
    unsigned mFFTSize;
    unsigned mHopSize;

    Array< CSine<T> >             mOsc;
    Array< OnePole<T,T,Td> >      mAmpSmooth;

    T mAmpScale   = T(1);
    T mMagThresh  = T(1e-5);
    T mSmoothTime = T(0.01); // seconds
};

} // namespace gam
