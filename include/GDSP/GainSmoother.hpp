template <class Sample>
class GainSmoother {
public:
    void setAttack(Sample seconds)  { atk = coeff(seconds); }
    void setRelease(Sample seconds) { rel = coeff(seconds); }

    void reset(Sample db = 0) { gainDb = db; }

    Sample process(Sample targetDb)
    {
        Sample coeffUsed = (targetDb < gainDb) ? atk : rel;
        gainDb = (1 - coeffUsed) * targetDb + coeffUsed * gainDb;
        return gainDb;
    }

    Sample valueDb() const { return gainDb; }

private:
    static Sample coeff(Sample sec)
    {
        sec = std::max((Sample)1e-6, sec);
        return std::exp(-1 / (sec * gam::sampleRate()));
    }

    Sample atk = 0.0, rel = 0.0;
    Sample gainDb = 0.0;
};
