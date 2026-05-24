class ExpGlide
{
public:
    ExpGlide(double startFreq, double timeSeconds, double sampleRate)
        : current(startFreq), target(startFreq)
    {
        setTime(timeSeconds, sampleRate);
    }

    void setTime(double seconds, double sampleRate)
    {
        const double tau = seconds;
        alpha = 1.0 - std::exp(-1.0 / (tau * sampleRate));
    }

    void setTarget(double freq) { target = freq; }

    double process()
    {
        current += alpha * (target - current);
        return current;
    }

private:
    double current = 0.0;
    double target = 0.0;
    double alpha = 0.0;
};
