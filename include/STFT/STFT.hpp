#pragma once
#include <Gamma/DFT.h>
#include <cstring>
#include <complex>
#include <vector>

class STFT
{
public:
    STFT(uint32_t window_size = 1024,
                uint32_t hop_size    = 256,
                uint32_t pad_size    = 0,
                gam::WindowType wType = gam::WindowType::HANN,
                gam::SpectralType sType = gam::SpectralType::COMPLEX)
    : stft(window_size, hop_size, pad_size, wType, sType)
    {
        input.resize(window_size);
        output.resize(stft.sizeDFT());
        spectrum.resize(stft.numBins());
    }

    // ------------------ Analysis ------------------
    void forward(const float* in)
    {
        std::memcpy(input.data(), in, sizeof(float) * input.size());
        stft.forward(input.data());
        pullSpectrum();
    }

    // ------------------ Synthesis -----------------
    void inverse()
    {
        pushSpectrum();
        stft.inverse(output.data());
    }

    // ---------------- Spectrum Access -------------
    std::complex<float>*       bins()       { return spectrum.data(); }
    const std::complex<float>* bins() const { return spectrum.data(); }

    uint32_t numBins() const { return spectrum.size(); }

    const float* getOutput() const { return output.data(); }

private:
    gam::STFT stft;

    std::vector<float> input;
    std::vector<float> output;

    std::vector<std::complex<float>> spectrum;

    // ---- Conversion: Gamma -> std ----
    void pullSpectrum()
    {
        auto* g = stft.bins();
        for (uint32_t i = 0; i < spectrum.size(); ++i)
            spectrum[i] = { g[i].real(), g[i].imag() };
    }

    // ---- Conversion: std -> Gamma ----
    void pushSpectrum()
    {
        auto* g = stft.bins();
        for (uint32_t i = 0; i < spectrum.size(); ++i)
            g[i] = gam::Complex<float>( spectrum[i].real(), spectrum[i].imag() );
    }
};
