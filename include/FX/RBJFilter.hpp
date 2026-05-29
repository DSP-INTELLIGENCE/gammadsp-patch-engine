#pragma once
#include <cmath>
#include <algorithm>

#include "Engine.hpp"
#include "Parameters.hpp"

template<class Sample>
class RBJFilter : public Function {
public:
    enum Type {
        LPF, HPF, BPF_SKIRT_Q, BPF_PEAK_0DB, NOTCH, ALLPASS,
        PEAKING, LOW_SHELF, HIGH_SHELF, THRU
    };

    RBJFilter(Sample sampleRate,
              Type type = Type::LPF,
              Sample freqHz = Sample(1000),
              Sample q = Sample(0.707),
              Sample gainDb = Sample(0),
              Sample shelfSlope = Sample(1))
    : mSR(sampleRate), mType(type),
      freq((float)freqHz), Q((float)q), gainDb((float)gainDb), shelfSlope((float)shelfSlope)
    {
        updateCoeffs(freqHz, q, gainDb, shelfSlope);
    }

    void setType(Type t) {
        mType = t;
        updateCoeffs((Sample)freq.process(), (Sample)Q.process(),
                    (Sample)gainDb.process(), (Sample)shelfSlope.process());
    }


    void setFreq(Sample f) {
        const Sample ff = std::clamp(f, Sample(1), Sample(0.45) * mSR);
        freq.set((float)ff);
    }

    void setQ(Sample q) {
        const Sample qq = std::max(Sample(0.001), q);
        Q.set((float)qq);
    }

    void setGainDb(Sample db) { gainDb.set((float)db); }

    void setShelfSlope(Sample s) {
        const Sample ss = std::max(Sample(0.001), s);
        shelfSlope.set((float)ss);
    }

    void reset() { z1 = z2 = Sample(0); }

    Sample process(Sample x) override
    {
        const Sample f = (Sample)freq.process();
        const Sample q = (Sample)Q.process();
        const Sample g = (Sample)gainDb.process();
        const Sample s = (Sample)shelfSlope.process();

        const bool need = freq.isRamping() || Q.isRamping() || gainDb.isRamping() || shelfSlope.isRamping();
        if (need) updateCoeffs(f, q, g, s);

        const Sample y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;

        // Denormal guard
        if (std::abs((double)z1) < 1e-20) z1 = Sample(0);
        if (std::abs((double)z2) < 1e-20) z2 = Sample(0);

        return y;
    }

    void run(const Sample* in, Sample* out, size_t n) {
        for (size_t i = 0; i < n; ++i) out[i] = process(in[i]);
    }

private:
    void updateCoeffs(Sample f0, Sample q, Sample dBgain, Sample S)
    {
        const Sample w0 = Sample(2) * Sample(M_PI) * f0 / mSR;
        const Sample cw = std::cos(w0);
        const Sample sw = std::sin(w0);

        const Sample A  = std::pow(Sample(10), dBgain / Sample(40));

        Sample alpha = Sample(0);

        // locals (avoid shadowing members)
        Sample bb0 = Sample(1), bb1 = Sample(0), bb2 = Sample(0);
        Sample aa0 = Sample(1), aa1 = Sample(0), aa2 = Sample(0);

        switch (mType)
        {
        case LPF:
            alpha = sw / (Sample(2) * q);
            bb0 = (Sample(1) - cw) * Sample(0.5);
            bb1 =  Sample(1) - cw;
            bb2 = (Sample(1) - cw) * Sample(0.5);
            aa0 =  Sample(1) + alpha;
            aa1 = -Sample(2) * cw;
            aa2 =  Sample(1) - alpha;
            break;

        case HPF:
            alpha = sw / (Sample(2) * q);
            bb0 = (Sample(1) + cw) * Sample(0.5);
            bb1 = -(Sample(1) + cw);
            bb2 = (Sample(1) + cw) * Sample(0.5);
            aa0 =  Sample(1) + alpha;
            aa1 = -Sample(2) * cw;
            aa2 =  Sample(1) - alpha;
            break;

        case BPF_SKIRT_Q:
            alpha = sw / (Sample(2) * q);
            bb0 =  sw * Sample(0.5);
            bb1 =  Sample(0);
            bb2 = -sw * Sample(0.5);
            aa0 =  Sample(1) + alpha;
            aa1 = -Sample(2) * cw;
            aa2 =  Sample(1) - alpha;
            break;

        case BPF_PEAK_0DB:
            alpha = sw / (Sample(2) * q);
            bb0 =  alpha;
            bb1 =  Sample(0);
            bb2 = -alpha;
            aa0 =  Sample(1) + alpha;
            aa1 = -Sample(2) * cw;
            aa2 =  Sample(1) - alpha;
            break;

        case NOTCH:
            alpha = sw / (Sample(2) * q);
            bb0 =  Sample(1);
            bb1 = -Sample(2) * cw;
            bb2 =  Sample(1);
            aa0 =  Sample(1) + alpha;
            aa1 = -Sample(2) * cw;
            aa2 =  Sample(1) - alpha;
            break;

        case ALLPASS:
            alpha = sw / (Sample(2) * q);
            bb0 =  Sample(1) - alpha;
            bb1 = -Sample(2) * cw;
            bb2 =  Sample(1) + alpha;
            aa0 =  Sample(1) + alpha;
            aa1 = -Sample(2) * cw;
            aa2 =  Sample(1) - alpha;
            break;

        case PEAKING:
            alpha = sw / (Sample(2) * q);
            bb0 =  Sample(1) + alpha * A;
            bb1 = -Sample(2) * cw;
            bb2 =  Sample(1) - alpha * A;
            aa0 =  Sample(1) + alpha / A;
            aa1 = -Sample(2) * cw;
            aa2 =  Sample(1) - alpha / A;
            break;

        case LOW_SHELF:
        {
            const Sample term = (A + Sample(1) / A) * (Sample(1) / S - Sample(1)) + Sample(2);
            alpha = (sw * Sample(0.5)) * std::sqrt(std::max(Sample(0), term));
            const Sample twoSqrtAalpha = Sample(2) * std::sqrt(A) * alpha;

            bb0 =    A * ((A + Sample(1)) - (A - Sample(1)) * cw + twoSqrtAalpha);
            bb1 =  Sample(2) * A * ((A - Sample(1)) - (A + Sample(1)) * cw);
            bb2 =    A * ((A + Sample(1)) - (A - Sample(1)) * cw - twoSqrtAalpha);
            aa0 =        (A + Sample(1)) + (A - Sample(1)) * cw + twoSqrtAalpha;
            aa1 =   -Sample(2) * ((A - Sample(1)) + (A + Sample(1)) * cw);
            aa2 =        (A + Sample(1)) + (A - Sample(1)) * cw - twoSqrtAalpha;
            break;
        }

        case HIGH_SHELF:
        {
            const Sample term = (A + Sample(1) / A) * (Sample(1) / S - Sample(1)) + Sample(2);
            alpha = (sw * Sample(0.5)) * std::sqrt(std::max(Sample(0), term));
            const Sample twoSqrtAalpha = Sample(2) * std::sqrt(A) * alpha;

            bb0 =    A * ((A + Sample(1)) + (A - Sample(1)) * cw + twoSqrtAalpha);
            bb1 = -Sample(2) * A * ((A - Sample(1)) + (A + Sample(1)) * cw);
            bb2 =    A * ((A + Sample(1)) + (A - Sample(1)) * cw - twoSqrtAalpha);
            aa0 =        (A + Sample(1)) - (A - Sample(1)) * cw + twoSqrtAalpha;
            aa1 =    Sample(2) * ((A - Sample(1)) - (A + Sample(1)) * cw);
            aa2 =        (A + Sample(1)) - (A - Sample(1)) * cw - twoSqrtAalpha;
            break;
        }

        case THRU:
        default:
            bb0 = Sample(1); bb1 = Sample(0); bb2 = Sample(0);
            aa0 = Sample(1); aa1 = Sample(0); aa2 = Sample(0);
            break;
        }

        const Sample invA0 = Sample(1) / aa0;

        // ✅ write to members
        b0 = bb0 * invA0;
        b1 = bb1 * invA0;
        b2 = bb2 * invA0;
        a1 = aa1 * invA0;
        a2 = aa2 * invA0;
    }

private:
    Sample mSR;
    Type   mType;

    // coefficients + state
    Sample b0 = Sample(1), b1 = Sample(0), b2 = Sample(0);
    Sample a1 = Sample(0), a2 = Sample(0);
    Sample z1 = Sample(0), z2 = Sample(0);

    // smoothed params (float-based)
    ParamLinear freq;
    ParamLinear Q;
    ParamLinear gainDb;
    ParamLinear shelfSlope;
};

