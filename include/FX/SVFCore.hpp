#pragma once
#include <cmath>
#include <algorithm>
#include "GDSP_Parameters.hpp"

template<class Sample>
class TVSVF {
public:
    enum Type { LP, BP, HP, NOTCH, PEAK, ALL };

    TVSVF(float fc, float r)
    {        
        setCutoff(fc);
        setQ(q);
    }

    void setType(Type t) { type = t; }

    void setCutoff(Sample f)
    {
        double sr = gam::sampleRate();
        f = std::clamp(f, Sample(0.001f) * sr, Sample(0.45) * sr);
        cutoff = f;
    }

    void setQ(Sample q)
    {
        q = std::clamp(q, Sample(0.05), Sample(50));
        Q = q;
    }

    void reset()
    {
        z1[0] = z1[1] = Sample(0);
        z2[0] = z2[1] = Sample(0);
    }

    inline Sample process(Sample x, int ch = 0)
    {
        updateCoeffs((Sample)cutoff, (Sample)Q);
        
        const Sample hp = (x - k * z1[ch] - z2[ch]) * a;
        const Sample bp = g * hp + z1[ch];
        const Sample lp = g * bp + z2[ch];

        z1[ch] = bp;
        z2[ch] = lp;

        if (std::abs((double)z1[ch]) < 1e-20) z1[ch] = Sample(0);
        if (std::abs((double)z2[ch]) < 1e-20) z2[ch] = Sample(0);

        switch (type)
        {
            case LP:    return lp;
            case BP:    return bp;
            case HP:    return hp;
            case NOTCH: return x - k * bp;
            case PEAK:  return lp - hp;
            case ALL:   return x - Sample(2) * k * bp;
            default:    return lp;
        }
    }

private:
    void updateCoeffs(Sample f, Sample q)
    {
        const Sample fc = f / gam::sampleRate();
        g = std::tan(Sample(M_PI) * fc);
        k = Sample(1) / q;
        a = Sample(1) / (Sample(1) + g * (g + k));
    }

private:
    Type type = LP;

    // Smoothed parameters
    Sample cutoff;
    Sample Q;

    // Coefficients
    Sample g = Sample(0);
    Sample k = Sample(0);
    Sample a = Sample(1);

    // State (stereo safe)
    Sample z1[2] {0, 0};
    Sample z2[2] {0, 0};
};
