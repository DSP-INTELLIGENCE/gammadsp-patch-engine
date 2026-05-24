#include "TPT1Pole.hpp"
#include "OTA1Pole.hpp"

struct MS20Filter
{
    float sr = 44100.f;

    // Two OTA-based stages: HPF -> LPF
    OTA_TPT1Pole hpf1, hpf2;
    OTA_TPT1Pole lpf1, lpf2;

    // Envelope & bias modeling
    TPT1Pole envLP;
    TPT1Pole biasLP;
    float bias = 0.f;

    // Controls
    float cutoff = 1000.f;
    float resonance = 0.6f;
    float drive = 1.5f;

    float k = 0.f;

    void init(float fs)
    {
        sr = fs;
        envLP.setLPCut(20.f, sr);
        biasLP.setLPCut(2.f, sr);
        reset();
        set(cutoff, resonance);
    }

    void reset()
    {
        hpf1.reset(); hpf2.reset();
        lpf1.reset(); lpf2.reset();
        envLP.reset(); biasLP.reset();
        bias = 0.f;
    }

    void set(float fc, float res)
    {
        cutoff = fc;
        resonance = std::clamp(res, 0.f, 1.2f);
        k = 3.5f * resonance; // MS-20 is hotter than Moog

        hpf1.setCut(fc, sr);
        hpf2.setCut(fc, sr);
        lpf1.setCut(fc, sr);
        lpf2.setCut(fc, sr);
    }

    inline float process(float x)
    {
        // Envelope → bias drift (MS-20 has nasty bias movement)
        float env = envLP.processLP(std::fabs(x));
        bias = biasLP.processLP(-0.3f * env);

        // Nonlinear input drive
        x = OTA_TPT1Pole::ota(x * drive + bias);

        // HPF stage (2-pole OTA)
        float h = hpf1.process(x);
        h = hpf2.process(h);

        // Aggressive nonlinear feedback
        float fb = OTA_TPT1Pole::ota(p4.s) + bias;

        // LPF stage (2-pole OTA with screaming resonance)
        float u = h - k * fb;

        float l = lpf1.process(u);
        l = lpf2.process(l);

        return l;
    }
};
