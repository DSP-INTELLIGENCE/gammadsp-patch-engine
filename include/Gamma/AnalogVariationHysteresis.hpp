#pragma once
template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
struct AnalogVariationHysteresisSat : public Td
{
    // --- static component tolerances
    T cutoffTol = T(1);
    T resTol    = T(1);
    T driveTol  = T(1);

    // --- thermal state
    T temp = T(0);

    // --- stochastic drift
    T rw = T(0);

    // --- signal power
    T power = T(0);

    // --- controls (time-domain)
    T heatGain = T(0.03);
    T heatBW   = T(1.0);     // Hz
    T coolBW   = T(0.08);    // Hz
    T driftBW  = T(0.005);   // Hz

    // --- saturation
    T tMax = T(1.0);
    T tSat = T(0.6);

    // --- cached coefficients (sample-rate dependent)
    T pCoef     = T(0);
    T heatK     = T(0);
    T coolK     = T(0);
    T driftLeak = T(0);
    T driftDiff = T(0);

    // RNG
    uint32_t rng = 1;

    void reset(uint32_t seed = 1)
    {
        rng = seed;
        temp = rw = power = T(0);

        cutoffTol = T(1) + uniform(-T(0.015), T(0.015));
        resTol    = T(1) + uniform(-T(0.020), T(0.020));
        driveTol  = T(1) + uniform(-T(0.020), T(0.020));
    }

    /// Gamma domain callback
    void onDomainChange(double /*r*/)
    {
        // Power envelope (~5 Hz)
        pCoef = T(1) - std::exp(-Td::ups() * T(5.0));

        // OU drift
        driftLeak = std::exp(-Td::ups() * driftBW);
        driftDiff = std::sqrt(std::max(T(0), T(1) - driftLeak * driftLeak));

        // Hysteresis dynamics
        heatK = T(1) - std::exp(-Td::ups() * heatBW);
        coolK = T(1) - std::exp(-Td::ups() * coolBW);
    }

    /// Call once per sample with signal value
    inline T process(T signal)
    {
        // --- RMS-ish power
        power += pCoef * (signal * signal - power);

        // --- stochastic drift
        rw = driftLeak * rw + driftDiff * white();

        // --- diminishing heating efficiency
        T ts = (tSat > T(0)) ? tSat : T(1);
        T u  = temp / ts;
        T eff = T(1) / (T(1) + u*u);

        // --- target temperature
        T target = (heatGain * eff) * power + rw;

        // --- hysteresis update
        T k = (target > temp) ? heatK : coolK;
        temp += k * (target - temp);

        // --- soft saturation
        if(tMax > T(0))
            temp = tMax * std::tanh(temp / tMax);

        return temp;
    }

    float operator()(float input) { return process(input); }
    
private:
    inline T white()
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return T(int32_t(rng)) * T(4.656612873e-10);
    }

    inline T uniform(T a, T b)
    {
        return a + (b - a) * (white() * T(0.5) + T(0.5));
    }
};
