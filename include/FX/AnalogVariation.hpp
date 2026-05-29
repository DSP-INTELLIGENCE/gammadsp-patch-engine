template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
struct AnalogVariation : public Td
{
    // dynamic thermal state
    T temp = T(0);

    // static component tolerances
    T cutoffTol = T(1);
    T resTol    = T(1);
    T driveTol  = T(1);

    // internal random walk state
    T rw = T(0);

    // RNG
    uint32_t rng = 1;

    void reset(uint32_t seed = 1)
    {
        rng = seed;
        temp = rw = T(0);

        // 1–2% component tolerances
        cutoffTol = T(1) + uniform(-T(0.015), T(0.015));
        resTol    = T(1) + uniform(-T(0.020), T(0.020));
        driveTol  = T(1) + uniform(-T(0.020), T(0.020));
    }

    inline T process()
    {
        // drift bandwidth (~0.01 Hz thermal wander)
        T bw = T(0.01);

        // convert to OU coefficients
        T leak = std::exp(-Td::ups() * bw);
        T diff = std::sqrt(T(1) - leak * leak);

        // band-limited random walk
        rw = leak * rw + diff * white();

        // very slow thermal inertia
        T k = T(1) - std::exp(-Td::ups() * T(0.002));
        temp += k * (rw - temp);

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
