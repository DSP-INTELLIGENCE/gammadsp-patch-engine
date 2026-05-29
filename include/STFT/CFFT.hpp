class CFFT
{
public:
    explicit CFFT(size_t n = 0)
    {
        resize(n);
    }

    void resize(size_t n)
    {
        size_ = n;
        fft.resize((int)n);

        freq.resize(n);
        raw.resize(2 * n);
    }

    size_t size() const { return size_; }

    std::complex<float>*       spectrum()       { return freq.data(); }
    const std::complex<float>* spectrum() const { return freq.data(); }

    // ---------------- Core API ----------------

    void forward(const std::complex<float>* input)
    {
        for (size_t i = 0; i < size_; ++i) {
            raw[2*i]     = input[i].real();
            raw[2*i + 1] = input[i].imag();
        }

        fft.forward(raw.data(), true);

        unpackFreq();
    }

    void inverse(std::complex<float>* output)
    {
        packFreq();
        fft.inverse(raw.data());
        unpackToOutput(output);
    }

private:
    size_t size_ = 0;

    gam::CFFT<float> fft;

    std::vector<std::complex<float>> freq;
    std::vector<float> raw;

    void unpackFreq()
    {
        for (size_t i = 0; i < size_; ++i)
            freq[i] = { raw[2*i], raw[2*i + 1] };
    }

    void packFreq()
    {
        for (size_t i = 0; i < size_; ++i) {
            raw[2*i]     = freq[i].real();
            raw[2*i + 1] = freq[i].imag();
        }
    }

    void unpackToOutput(std::complex<float>* output)
    {
        for (size_t i = 0; i < size_; ++i)
            output[i] = freq[i];
    }
};
