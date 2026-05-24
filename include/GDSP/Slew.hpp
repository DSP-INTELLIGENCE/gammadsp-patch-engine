class Slew
{
public:
    Slew(double value = 0.0) : current(value), target(value) {}

    void setTime(double seconds, double sampleRate)
    {
        const double tau = std::max(1e-6, seconds);
        alpha = 1.0 - std::exp(-1.0 / (tau * sampleRate));
    }

    void setTarget(double value)
    {
        target = value;
    }

    inline double process()
    {
        current += alpha * (target - current);
        return current;
    }

    double get() const { return current; }

private:
    double current = 0.0;
    double target = 0.0;
    double alpha = 0.0;
};
