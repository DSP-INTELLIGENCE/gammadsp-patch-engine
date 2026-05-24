#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class AdaptiveGainComputer : public Td {
public:
    AdaptiveGainComputer()
    {
        reset();
    }

    // ---------------- base parameters ----------------

    void setThresholdDb(T db) { threshDb = db; }

    void setBaseRatio(T r)    { baseRatio = scl::max(r, T(1)); }
    void setMaxRatio(T r)     { maxRatio  = scl::max(r, baseRatio); }

    void setBaseKneeDb(T db)  { baseKnee = scl::max(db, T(0)); }
    void setMaxKneeDb(T db)   { maxKnee  = scl::max(db, baseKnee); }

    /// 0 = static compressor, 1 = fully adaptive
    void setAdaptAmount(T a)  { adapt = scl::clip(a, T(0), T(1)); }

    /// Reference error where adaptation saturates (dB)
    void setErrorRefDb(T db)  { errRef = scl::max(db, T(1)); }

    void reset()
    {
        lastGainDb = T(0);
    }

    // ---------------- processing ----------------

    /// Input: level in dB
    /// Output: target gain in dB (≤ 0)
    T operator()(T levelDb)
    {
        T d = levelDb - threshDb;
        if (d <= T(0))
            return T(0);

        // --- Normalize error ---
        T eNorm = scl::min(T(1), d / errRef);

        // --- Adaptive ratio ---
        T ratio = baseRatio +
                  adapt * eNorm * (maxRatio - baseRatio);

        // --- Adaptive knee ---
        T knee = baseKnee +
                 adapt * (T(1) - eNorm) * (maxKnee - baseKnee);

        // --- Soft knee compression ---
        T gainDb = T(0);

        if (knee > T(0))
        {
            T halfK = knee * T(0.5);

            if (d <= -halfK)
            {
                gainDb = T(0);
            }
            else if (d >= halfK)
            {
                gainDb = -d * (T(1) - T(1) / ratio);
            }
            else
            {
                // Quadratic knee
                T x = d + halfK;
                T y = x * x / (T(2) * knee);
                gainDb = -y * (T(1) - T(1) / ratio);
            }
        }
        else
        {
            // Hard knee
            gainDb = -d * (T(1) - T(1) / ratio);
        }

        lastGainDb = gainDb;
        return gainDb;
    }

    // ---------------- diagnostics ----------------

    T lastGain() const { return lastGainDb; }

private:
    // Base parameters
    T threshDb   { T(-24) };
    T baseRatio  { T(4) };
    T maxRatio   { T(12) };
    T baseKnee   { T(3) };
    T maxKnee    { T(12) };

    // Adaptation
    T adapt  { T(0.5) };
    T errRef { T(12) };

    // State
    T lastGainDb { T(0) };
};

} // namespace gam
