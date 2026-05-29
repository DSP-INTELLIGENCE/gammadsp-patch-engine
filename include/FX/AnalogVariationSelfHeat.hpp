template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
struct AnalogVariationSelfHeat : public Td
{
    // --- static component tolerances
    T cutoffTol = T(1);
    T resTol    = T(1);
    T driveTol  = T(1);

    // --- thermal state
    T temp = T(0);

    // --- random drift state
    T rw = T(0);

    // --- signal power state
    T power = T(0);

    // --- controls
    T heatGain = T(0.02);   // how much signal heats components
    T coolRate = T(0.01);   // cooling speed (Hz)
    T driftBW  = T(0.005);  // thermal noise bandwidth (Hz)

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

    /// Call once per sample with the *signal being processed*
    inline T process(T signal)
    {
        // --- signal power estimation (RMS-ish)
        T pCoef = T(1) - std::exp(-Td::ups() * T(5.0)); // ~5 Hz envelope
        power += pCoef * (signal * signal - power);

        // --- stochastic thermal drift (OU process)
        T leak = std::exp(-Td::ups() * driftBW);
        T diff = std::sqrt(T(1) - leak * leak);
        rw = leak * rw + diff * white();

        // --- thermal integration
        T heat = heatGain * power;
        T cool = coolRate * temp;

        T kTherm = T(1) - std::exp(-Td::ups() * T(0.5)); // thermal inertia
        temp += kTherm * ((heat + rw) - cool);

        return temp;
    }

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
