///////////////////////////////////////////////////////////////
// Unison PolyBlep
///////////////////////////////////////////////////////////////

class Unison : public Generator {
public:
    Unison(int voices = 7, float freq = 440.f);

    float process() override;

    // core
    void setVoices(int n);                 // 1..maxVoices
    void setFreq(float f);
    void setPhase(float p);                // base phase (radians)
    void setWaveform(Oscillator::Waveform w);
    void setPulseWidth(float pw);

    // unison controls
    void setDetuneCents(float cents);      // e.g. 0..50
    void setStereoSpread(float s);         // 0..1 (only meaningful if you output stereo elsewhere)
    void setRandomPhase(bool e);           // randomize phases on voice count change / reset

    // modulation (shared)
    void setFM(float v) { fm.set(v); }
    void setPM(float v) { pm.set(v); }
    void setAM(float v) { am.set(v); }
    
    Modulator fm;
    Modulator pm;
    Modulator am;

private:
    static constexpr int maxVoices = 16;

    Oscillator mVoices[maxVoices];
    int mNumVoices = 1;

    float detuneCents = 0.f;
    float stereoSpread = 0.f;
    bool randomPhase = true;

    // cached per-voice detune multipliers
    double detuneMul[maxVoices]{};

    void recacheDetune();
    static double centsToMul(double cents) { return std::pow(2.0, cents / 1200.0); }
};

