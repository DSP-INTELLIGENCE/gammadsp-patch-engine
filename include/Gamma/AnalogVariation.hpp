template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
struct AnalogVariation : public Td
{
    // --- dynamic thermal state
    T temp = T(0);

    // --- static component tolerances
    T cutoffTol = T(1);
    T resTol    = T(1);
    T driveTol  = T(1);

    // --- random walk state
    T rw = T(0);

    // --- parameters (Hz)
    T driftBW   = T(0.01);   // thermal wander bandwidth
    T inertiaBW = T(0.002);  // thermal mass response

    // --- cached coefficients (SR dependent)
    T driftLeak = T(0);
    T driftDiff = T(0);
    T inertiaK  = T(0);

    // RNG
    uint32_t rng = 1;

    AnalogVariation()
    {
        onDomainChange(1);
    }

    void reset(uint32_t seed = 1)
    {
        rng = seed;
        temp = rw = T(0);

        // 1–2% component tolerances
        cutoffTol = T(1) + uniform(-T(0.015), T(0.015));
        resTol    = T(1) + uniform(-T(0.020), T(0.020));
        driveTol  = T(1) + uniform(-T(0.020), T(0.020));
    }

    /// Gamma domain callback
    void onDomainChange(double /*r*/)
    {
        // OU drift coefficients
        driftLeak = std::exp(-Td::ups() * driftBW);
        driftDiff = std::sqrt(std::max(T(0), T(1) - driftLeak * driftLeak));

        // thermal inertia
        inertiaK = T(1) - std::exp(-Td::ups() * inertiaBW);
    }

    inline T process()
    {
        // --- band-limited random walk (OU process)
        rw = driftLeak * rw + driftDiff * white();

        // --- thermal mass integration
        temp += inertiaK * (rw - temp);

        return temp;
    }

private:
    inline T white()
    {
        // xorshift32
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return T(int32_t(rng)) * T(4.656612873e-10); // [-1,1]
    }

    inline T uniform(T a, T b)
    {
        return a + (b - a) * (white() * T(0.5) + T(0.5));
    }
};
