#include "GDSP_Oscillators.hpp"

class SweepOscillator : public Generator {
public:

    enum class Waveform {
        Sine,
        Saw,
        Square,
        Triangle,
        Pulse
    };

    SweepOscillator(float freq = 440.0f, Waveform wf = Waveform::Sine)
        : mWaveform(wf), mPulseWidth(0.5f)
    {
        mSweep.setFreq(freq);
    }

    void setFreq(float f) { mSweep.setFreq(f); }
    void setPhase(float p) { mSweep.setPhase(p); }
    void reset() { mSweep.reset(); }

    void setWaveform(Waveform wf) { mWaveform = wf; }
    void setPulseWidth(float pw) { mPulseWidth = std::clamp(pw, 0.01f, 0.99f); }

    float process() override {
        float p = mSweep.process();
        return render(p);
    }

    float processFM(float fm) {
        float p = mSweep.processFM(fm);
        return render(p);
    }

private:
    float render(float p) {
        switch (mWaveform) {
        case Waveform::Sine:
            return std::sin(2.0f * M_PI * p);

        case Waveform::Saw:
            return 2.0f * p - 1.0f;

        case Waveform::Square:
            return p < mPulseWidth ? 1.0f : -1.0f;

        case Waveform::Triangle:
            return 4.0f * std::fabs(p - 0.5f) - 1.0f;

        case Waveform::Pulse:
            return p < mPulseWidth ? 1.0f : 0.0f;
        }
        return 0.0f;
    }

private:
    Sweep     mSweep;
    Waveform  mWaveform;
    float     mPulseWidth;
};

/*
float lfo = std::sin(2 * M_PI * lfoPhase);
float sample = osc.processFM(lfo * 5.0f);

Oscillator osc(220.0f, Waveform::Saw);

for (int i = 0; i < 48000; ++i) {
    float sample = osc.process();
    output[i] = sample * 0.3f;
}
*/