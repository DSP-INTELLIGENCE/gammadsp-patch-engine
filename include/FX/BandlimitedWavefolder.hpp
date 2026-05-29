template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN, int OS = 4>
struct BandlimitedWavefolder : public Td
{
    // --- parameters
    T drive     = T(1);
    T threshold = T(1);
    T mix       = T(1);

    // --- state
    T xPrev  = T(0);
    T carry  = T(0);   // BLAMP "next-sample" correction carried across calls

    // --- AA filters (called OS times per sample)
    TPT1Pole<T, Td> aa1, aa2;

    BandlimitedWavefolder()
    {
        onDomainChange(1.0);
        reset();
    }

    void reset()
    {
        xPrev = T(0);
        carry = T(0);
        aa1.reset();
        aa2.reset();
    }

    void onDomainChange(double /*r*/)
    {
        const T fs = T(Td::spu());
        if(fs <= T(0)) return;

        const T cutoff_hz = T(0.45) * (fs * T(0.5));   // ≈ 0.225*fs
        const T cutoff_design = cutoff_hz / T(OS);     // executed OS× => design fc/OS

        aa1.freq(cutoff_design);
        aa2.freq(cutoff_design);
    }

    inline T operator()(T x)
    {
        const T in0 = xPrev;
        const T in1 = x;
        xPrev = x;

        T y = T(0);

        T xLastSub = in0;

        // carry must persist across calls (last substep's "next" is next call's first substep)
        T carryLocal = carry;
        carry = T(0);

        for(int i = 1; i <= OS; ++i)
        {
            const T t  = T(i) / T(OS);
            const T xi = in0 + (in1 - in0) * t;

            const T xd0 = xLastSub * drive;
            const T xd1 = xi       * drive;

            // fold
            T v = foldReflect(xd1, threshold);

            // apply carry from previous substep (or previous call)
            v += carryLocal;
            carryLocal = T(0);

            // BLAMP correction for any kink(s) inside this OS interval
            T corrNow, corrNext;
            foldBLAMPApply(xd0, xd1, threshold, corrNow, corrNext);

            v += corrNow;
            carryLocal += corrNext;

            xLastSub = xi;

            // post-fold AA
            v = aa1(v);
            v = aa2(v);

            y = v;
        }

        // store carry for next call
        carry = carryLocal;

        return x + mix * (y - x);
    }

private:
    // ----------------------------
    // Reflective fold core
    // ----------------------------
    static inline T foldReflect(T x, T th)
    {
        if(th <= T(0)) return T(0);

        const T p = T(4) * th;
        T u = std::fmod(x + th, p);
        if(u < T(0)) u += p;

        const T tri = (u <= T(2) * th) ? u : (p - u);
        return tri - th; // in [-th, +th]
    }

    static inline T foldSlope(T x, T th)
    {
        const T p = T(4) * th;
        T u = std::fmod(x + th, p);
        if(u < T(0)) u += p;
        return (u < T(2) * th) ? T(1) : T(-1);
    }

    // ----------------------------
    // 2-point polyBLAMP split kernel
    // t in [0,1): event time within *this* (OS-rate) sample
    // blamp0 -> contribution to current sample
    // blamp1 -> contribution to next sample
    // ----------------------------
    static inline double blamp0(double t)
    {
        const double t2 = t * t;
        const double t3 = t2 * t;
        return (t3 / 6.0) - (t2 / 2.0) + (t / 2.0) - (1.0 / 6.0);
    }

    static inline double blamp1(double t)
    {
        const double u  = 1.0 - t;
        const double u2 = u * u;
        const double u3 = u2 * u;
        return (u3 / 6.0) - (u2 / 2.0) + (u / 2.0) - (1.0 / 6.0);
    }

    // ----------------------------
    // Apply BLAMP for fold-corner crossings in [x0, x1]
    // Returns corrNow (add to current OS sample) and corrNext (carry to next OS sample)
    // ----------------------------
    static inline void foldBLAMPApply(
        T x0, T x1, T th,
        T& corrNow,
        T& corrNext
    )
    {
        corrNow  = T(0);
        corrNext = T(0);
        if(th <= T(0)) return;

        const T dx = x1 - x0;
        if(dx == T(0)) return;

        T a = x0, b = x1;
        if(a > b) std::swap(a, b);

        const double amin = double(a / th);
        const double bmin = double(b / th);

        int n0 = int(std::ceil(amin));
        int n1 = int(std::floor(bmin));

        // corners at odd multiples: (2k+1)*th
        if((n0 & 1) == 0) ++n0;
        if((n1 & 1) == 0) --n1;
        if(n0 > n1) return;

        // slope before corner (piecewise linear; using x0 is fine for monotone segment)
        const T s0 = foldSlope(x0, th);

        for(int n = n0; n <= n1; n += 2)
        {
            const T c = T(n) * th;
            const T tT = (c - x0) / dx;      // local event time in (0,1) if crossing occurs
            const double t = double(tT);
            if(t <= 0.0 || t >= 1.0) continue;

            // At a fold corner, dy/dx flips sign: Δ(dy/dx) = -2*s0
            // In discrete time at OS rate, slope step in "per-sample" units:
            const T dM = (-T(2) * s0) * dx;

            corrNow  += dM * T(blamp0(t));
            corrNext += dM * T(blamp1(t));
        }
    }
};
