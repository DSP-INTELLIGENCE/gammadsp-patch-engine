#include "TPT1Pole.hpp"

struct OTA_VCA
{
    float sr = 44100.f;

    // Control
    float gain = 1.f;       // manual trim
    float bias = 0.f;       // operating point
    float temp = 1.f;       // temperature / softness
    float Iabc = 0.f;       // control current (0..~2)

    // Internal envelope smoothing (real circuits are never instant)
    TPT1Pole ctrlLP;

    void init(float fs)
    {
        sr = fs;
        ctrlLP.setLPCut(200.f, sr); // control port bandwidth
        reset();
    }

    void reset()
    {
        ctrlLP.reset();
    }

    // Set VCA level (0..1)
    void setLevel(float v01)
    {
        // Convert linear knob to exponential OTA current
        float I = std::pow(10.f, 2.f * v01) - 1.f;  // ~0..99
        Iabc = ctrlLP.processLP(I);
    }

    // OTA differential pair core
    inline float ota(float x)
    {
        // Differential pair I-V characteristic
        float v = (x + bias) / temp;
        return std::tanh(v) * Iabc;
    }

    inline float process(float x)
    {
        // Input transistor pair
        float y = ota(x);

        // Output scaling & gentle soft limiting
        y *= gain;
        return std::tanh(y);
    }
};

