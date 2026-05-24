template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
struct AnalogGainCell : public Td
{
    // --- conditioning ---
    TPT1Pole<T,Td> couplingHP;
    TPT1Pole<T,Td> dcBlock;

    // --- envelope paths ---
    TPT1Pole<T,Td> envFast;   // bias drive
    TPT1Pole<T,Td> envSlow;   // sag drive

    // --- parameters ---
    T drive      = T(1);
    T baseBias   = T(0);
    T biasAmount = T(0.3);
    T sagAmount  = T(0.2);

    // --- dynamics (Hz)
    T biasBW = T(5.0);        // bias memory speed

    // --- cached coeff ---
    T biasK = T(0);

    // --- state ---
    T bias = T(0);

    AnalogGainCell()
    {
        onDomainChange(1);
        reset();
    }

    void reset()
    {
        couplingHP.reset();
        dcBlock.reset();
        envFast.reset();
        envSlow.reset();
        bias = T(0);
    }

    /// Gamma domain callback
    void onDomainChange(double /*r*/)
    {
        // coupling & DC cleanup
        couplingHP.freq(T(10));
        dcBlock.freq(T(10));

        // envelope time constants
        envFast.freq(T(20));   // bias response
        envSlow.freq(T(2));    // supply sag

        // bias integrator (Hz → coefficient)
        biasK = T(1) - std::exp(-Td::ups() * biasBW);
    }

    inline T operator()(T x)
    {
        // --- AC coupling
        x = couplingHP.hp(x);

        // --- nonlinear core
        T v = x * drive + bias;
        T y = std::tanh(v);

        // --- envelope from post-nonlinearity
        T env = scl::abs(y);

        // --- bias memory (medium speed)
        T targetBias = baseBias - biasAmount * envFast(env);
        bias += biasK * (targetBias - bias);

        // --- supply sag (slow)
        T sag = T(1) / (T(1) + sagAmount * envSlow(env));
        y *= sag;

        // --- DC cleanup
        return dcBlock.hp(y);
    }
};
