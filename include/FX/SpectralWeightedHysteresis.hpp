#pragma once
#include <algorithm>
#include <cmath>
#include "RBJFilter.hpp"

template <class Sample>
class SpectralWeightedHysteresis {
public:
    SpectralWeightedHysteresis(Sample sampleRate)
    : low(sampleRate, RBJFilter<Sample>::LOWPASS),
      high(sampleRate, RBJFilter<Sample>::HIGHPASS)
    {
        low.setFreq((Sample)300);
        high.setFreq((Sample)3000);

        setWeights((Sample)0.6, (Sample)0.3, (Sample)0.1);
        setRange((Sample)0.15, (Sample)0.85);
        setCurveRange((Sample)0.8, (Sample)2.2);
    }

    void setWeights(Sample wL, Sample wM, Sample wH)
    {
        wLow = wL; wMid = wM; wHigh = wH;
        normalizeWeights();
    }

    void setRange(Sample minAmt, Sample maxAmt)
    {
        minAmount = std::clamp(minAmt, (Sample)0, (Sample)0.99);
        maxAmount = std::clamp(maxAmt, (Sample)0, (Sample)0.99);
    }

    void setCurveRange(Sample lo, Sample hi)
    {
        curveLo = std::max((Sample)0.1, lo);
        curveHi = std::max(curveLo, hi);
    }

    void reset()
    {
        low.reset(); high.reset();
        lowEnv = midEnv = highEnv = state = 0;
    }

    inline Sample process(Sample input, Sample env)
    {
        // Approximate spectral envelope
        Sample lowE  = std::abs(low.process(env));
        Sample highE = std::abs(high.process(env));
        Sample midE  = std::abs(env - lowE - highE);

        // Normalize spectral distribution
        Sample total = lowE + midE + highE + (Sample)1e-9;
        lowEnv  = lowE  / total;
        midEnv  = midE  / total;
        highEnv = highE / total;

        // Spectral influence on curve & memory
        Sample spectralFactor = wLow * lowEnv + wMid * midEnv + wHigh * highEnv;

        Sample curve = curveLo + (curveHi - curveLo) * spectralFactor;

        Sample dyn = minAmount + (maxAmount - minAmount) *
                     ((Sample)1 - std::pow(env, curve));

        // Apply hysteresis
        state = state * dyn + input * ((Sample)1 - dyn);
        return state;
    }

private:
    void normalizeWeights()
    {
        Sample s = wLow + wMid + wHigh + (Sample)1e-9;
        wLow /= s; wMid /= s; wHigh /= s;
    }

    RBJFilter<Sample> low, high;

    Sample wLow { 0.6 }, wMid { 0.3 }, wHigh { 0.1 };

    Sample minAmount { 0.15 };
    Sample maxAmount { 0.85 };

    Sample curveLo { 0.8 };
    Sample curveHi { 2.2 };

    Sample lowEnv { 0 }, midEnv { 0 }, highEnv { 0 };
    Sample state  { 0 };
};
