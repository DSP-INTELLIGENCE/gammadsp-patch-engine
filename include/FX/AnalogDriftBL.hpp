template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
struct AnalogDriftBL : public Td
{
    // --- user controls
    T amount = T(0.001);   // max drift magnitude
    T speed  = T(0.1);     // drift bandwidth (Hz)

    // --- internal state
    T x   = T(0);          // random walk
    T xLP = T(0);          // inertia stage

    // --- smoothed parameters
    T amtLP = amount;
    T spdLP = speed;

    // --- cached coefficients (SR dependent)
    T kParam    = T(0);
    T leak      = T(0);
    T diff      = T(0);
    T kInertia  = T(0);

    // RNG
    uint32_t rng = 1;

    AnalogDriftBL() {}

    /// Gamma domain callback
    void onDomainChange(double /*r*/)
    {
        // parameter smoothing (~2 Hz)
        kParam = T(1) - std::exp(-Td::ups() * T(2.0));

        // inertia pole (~0.3 Hz)
        kInertia = T(1) - std::exp(-Td::ups() * T(0.3));

        // initial drift coefficients
        updateDriftCoeffs();
    }

    /// Call this when speed (or smoothed speed) changes significantly
    inline void updateDriftCoeffs()
    {
        leak = std::exp(-Td::ups() * spdLP);
        diff = std::sqrt(std::max(T(0), T(1) - leak * leak));
    }

    inline T process()
    {
        // --- smooth user controls
        amtLP += kParam * (amount - amtLP);
        T prevSpd = spdLP;
        spdLP += kParam * (speed - spdLP);

        // --- update drift coeffs only if speed changed
        if(spdLP != prevSpd)
            updateDriftCoeffs();

        // --- OU random walk
        x = leak * x + diff * white();

        // --- extra inertia
        xLP += kInertia * (x - xLP);

        // --- scale and clamp
        T d = xLP * amtLP;
        return scl::clip(d, -amtLP, amtLP);
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
};
