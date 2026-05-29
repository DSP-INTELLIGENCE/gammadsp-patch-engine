#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class ExpansionCurve {
public:
    enum class Mode {
        Downward,   // expander / gate
        Upward      // upward expander
    };

    void setMode(Mode m)          { mode = m; }
    void setThresholdDb(Sample d){ T = d; }
    void setRatio(Sample r)       { R = std::max((Sample)1, r); }
    void setKneeDb(Sample k)      { K = std::max((Sample)0, k); }

    // Input: envelope (linear)
    // Output: target gain (dB)
    Sample process(Sample env) const
    {
        Sample L = (Sample)20 * std::log10(std::max(env, (Sample)1e-12));
        Sample d = L - T;

        // soft knee
        if (K > (Sample)0)
        {
            Sample halfK = K * (Sample)0.5;
            if (std::abs(d) < halfK)
            {
                d = (d + halfK) * (d + halfK) / ((Sample)2 * K);
            }
            else
            {
                d = std::max((Sample)0, d);
            }
        }

        if (mode == Mode::Downward)
        {
            if (d >= (Sample)0) return (Sample)0;
            return d * (Sample)(1 - R);   // attenuation
        }
        else // Upward
        {
            if (d <= (Sample)0) return (Sample)0;
            return d * (Sample)(R - 1);   // boost
        }
    }

private:
    Mode   mode { Mode::Downward };
    Sample T    { (Sample)-40 };
    Sample R    { (Sample)2 };
    Sample K    { (Sample)0 };
};
