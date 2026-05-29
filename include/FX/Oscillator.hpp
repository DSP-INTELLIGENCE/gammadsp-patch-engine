#pragma once
#include "Oscillators.hpp"

///////////////////////////////////////////////////////////////
// Oscillator
///////////////////////////////////////////////////////////////

class Oscillator : public Generator {
public:

    enum Waveform {
        SIN,
        SAW,
        SQUARE,
        TRI,
        PULSE,
        RECTANGLE,
    };

    Oscillator(float freq = 440.f);

    float process() override;

    void setFreq(float f);
    void setPhase(float p);              // expects [-1..1]
    void setWaveform(Waveform w);
    void setPulseWidth(float pw);
    void hardSync();
    void setFeedback(double f);
    void setRatio(double r);
    void setDetune(double d);
    void setThroughZeroFM(bool e);
    
    void setFM(float v) { fm.set(v); }
    void setPM(float v) { pm.set(v); }
    void setAM(float v) { am.set(v); }
    void setPWM(float v) { pw.set(v); }

    
    Modulator fm, pm, am, pw;

private:
    Waveform wave = SIN;

    double freq = 440.0;    
    double phase = 0.0;
    double lastOut = 0.0;
    double syncBlep = 0.0; 
    double pulseWidth = 0.5;
    double feedback = 0.0;
    
    double ratio  = 1.0;
    double detune = 0.0;
    bool   throughZeroFM = false;    
};


