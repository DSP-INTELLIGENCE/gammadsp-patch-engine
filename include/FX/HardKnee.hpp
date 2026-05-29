template <class Sample>
class HardKnee {
public:
    void setThreshold(Sample tDb) { T = tDb; }
    void setRatio(Sample r) { R = std::max((Sample)1, r); }

    // Returns gain reduction in dB (positive number)
    Sample process(Sample inputDb)
    {
        if (inputDb <= T) return (Sample)0;
        return (inputDb - T) * ((Sample)1 - (Sample)1 / R);
    }

private:
    Sample T { (Sample)-24 };
    Sample R { (Sample)4 };
};
