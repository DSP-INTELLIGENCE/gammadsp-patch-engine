class RFFT
{
public:
    explicit RFFT(size_t n = 0)
    {
        resize(n);
    }

    void resize(size_t n)
    {
        size_ = n;
        fft.resize((int)n);

        freq.resize(n / 2 + 1);
        raw.resize(n + 2);
    }

    size_t size()    const { return size_; }
    size_t numBins() const { return freq.size(); }

    std::complex<float>*       spectrum()       { return freq.data(); }
    const std::complex<float>* spectrum() const { return freq.data(); }

    // ---------------- Core API ----------------

    void forward(const float* input)
    {
        std::memcpy(raw.data(), input, size_ * sizeof(float));
        fft.forward(raw.data(), false, true);
        unpackFreq();
    }

    void inverse(float* output)
    {
        packFreq();
        fft.inverse(raw.data(), false);
        std::memcpy(output, raw.data(), size_ * sizeof(float));
    }

private:
    size_t size_ = 0;

    gam::RFFT<float> fft;

    std::vector<std::complex<float>> freq;
    std::vector<float> raw;

    void unpackFreq()
    {
        freq[0] = { raw[0], 0.0f };

        for (size_t k = 1; k < freq.size() - 1; ++k)
            freq[k] = { raw[2*k - 1], raw[2*k] };

        freq.back() = { raw[2*freq.size() - 2], 0.0f };
    }

    void packFreq()
    {
        raw[0] = freq[0].real();

        for (size_t k = 1; k < freq.size() - 1; ++k) {
            raw[2*k - 1] = freq[k].real();
            raw[2*k]     = freq[k].imag();
        }

        raw[2*freq.size() - 2] = freq.back().real();
    }
};
