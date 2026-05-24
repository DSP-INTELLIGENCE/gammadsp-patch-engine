#pragma once
#include <algorithm>
#include <vector>
#include <cmath>

enum class Mode { Downward, Upward, Gate };

template <class Sample>
class ThresholdComparator {
public:
    void setThreshold(Sample db) { T = db; }
    void setKnee(Sample db) { K = std::max((Sample)0, db); }
    void setMode(Mode m) { mode = m; }

    Sample process(Sample env)
    {
        env = std::max(env, (Sample)1e-12);
        Sample envDb = (Sample)20 * std::log10(env);
        Sample delta = envDb - T;

        Sample halfK = K * (Sample)0.5;
        Sample c = 0;

        if (K > 0) {
            if (delta > -halfK) {
                if (delta < halfK) {
                    Sample x = delta + halfK;
                    c = (x * x) / ((Sample)2 * K);
                } else {
                    c = delta;
                }
            }
        } else {
            if (delta > 0) c = delta;
        }

        if (mode == Mode::Upward)  return -c;

        if (mode == Mode::Gate) {
            if (K > 0 && delta > -halfK && delta < halfK) {
                Sample x = delta + halfK;
                return (x * x) / ((Sample)2 * K);
            }
            return (envDb < T) ? (T - envDb) : (Sample)0;
        }

        return c;  // Downward
    }

private:
    Sample T = (Sample)-24;
    Sample K = (Sample)6;
    Mode   mode = Mode::Downward;
};
