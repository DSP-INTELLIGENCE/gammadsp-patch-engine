template <class Sample>
class SoftKnee {
public:
    void setThreshold(Sample tDb) { T = tDb; }
    void setRatio(Sample r) { R = std::max((Sample)1, r); }
    void setKnee(Sample kDb) { K = std::max((Sample)0, kDb); }

    // returns gain reduction in dB (positive)
    Sample process(Sample inputDb)
    {
        if (K <= (Sample)0)
        {
            // hard knee
            if (inputDb <= T) return (Sample)0;
            return (inputDb - T) * ((Sample)1 - (Sample)1 / R);
        }

        Sample delta = inputDb - T;
        Sample halfK = K * (Sample)0.5;

        if (delta <= -halfK) return (Sample)0;

        if (delta >= halfK)
            return (delta - halfK) * ((Sample)1 - (Sample)1 / R)
                 + halfK * ((Sample)1 - (Sample)1 / R);

        // inside knee
        Sample x = delta + halfK;
        Sample y = (x * x) / (Sample)(2 * K);
        return y * ((Sample)1 - (Sample)1 / R);
    }

private:
    Sample T { (Sample)-24 };
    Sample R { (Sample)4 };
    Sample K { (Sample)6 };
};
