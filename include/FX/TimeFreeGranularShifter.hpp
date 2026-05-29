class TimeFreeGranularShifter : public Function {
public:
    TimeFreeGranularShifter(int grains = 8, int grainSize = 256)
    : N(grains), size(grainSize)
    {
        buffer.resize(size, 0.f);
        shifters.resize(N);
        phases.resize(N, 0.f);

        for(int i=0;i<N;i++)
            shifters[i].setShift((i - N/2) * 5.f);
    }

    float process(float x) override
    {
        buffer[write] = x;
        write = (write + 1) % size;

        float out = 0.f;

        for(int i=0;i<N;i++)
        {
            phases[i] += 1.f;
            if(phases[i] >= size) phases[i] = 0.f;

            float w = 0.5f - 0.5f * std::cos(2.f * M_PI * phases[i] / size);

            float g = buffer[(write - (int)phases[i] + size) % size];

            out += shifters[i].process(g * w);
        }

        return out / N;
    }

private:
    int N, size;
    int write = 0;

    std::vector<float> buffer;
    std::vector<FrequencyShifter> shifters;
    std::vector<float> phases;
};
