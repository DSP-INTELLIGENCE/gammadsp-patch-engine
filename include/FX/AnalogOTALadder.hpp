#include "OTALadder.hpp"


class AnalogOTALadder
{
public:

    AnalogOTALadder(float cutoff, float resonance)
    {
        float sr = gam::sampleRate();
        envLP.setLPCut(20.f, sr);
        biasLP.setLPCut(3.f, sr);
        reset();
        set(cutoff, resonance);
    }
    virtual ~AnalogOTALadder() = default;

    void reset()
    {
        p1.reset(); p2.reset(); p3.reset(); p4.reset();
        envLP.reset(); biasLP.reset();
        variation.reset();
        bias = 0.f;
    }

    void set(float fc, float res)
    {
        float sr = gam::sampleRate();
    
        float fcVar  = fc * variation.cutoffTol * (1.f + thermal);
        float resVar = res * variation.resTol;
        float drvVar = drive * variation.driveTol;

        cutoff    = std::clamp(fcVar, 0.0001f * sr, 0.499f * sr);
        resonance = std::clamp(resVar, 0.f, 1.2f);
        drive = drvVar;
        
        float gm = 1.f + 2.5f * drive;
        p1.gm = p2.gm = p3.gm = p4.gm = gm;
    
        k = 4.f * resonance;
        k = k / (1.f + 0.6f * k);
        k *= (0.5f + 0.5f * (cutoff / (0.5f * sr)));

        p1.setCut(cutoff, sr);
        p2.setCut(cutoff, sr);
        p3.setCut(cutoff, sr);
        p4.setCut(cutoff, sr);
    }

    inline float zap(float x){
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    inline float process(float x)
    {
        // --- envelope follower ---
        float env = envLP.processLP(std::fabs(x));
        // thermal & tolerance influence
        float thermal = variation.process();

        driftLP += 0.001f * (thermal - driftLP);
        float drift = driftLP;

        // --- dynamic bias shift ---
        float targetBias = -biasAmount * env;
        bias = biasLP.processLP(targetBias);
        
        // --- supply sag ---
        float sag = 1.f / (1.f + sagAmount * env);

        // --- nonlinear feedback path ---
        float fb = OTA_TPT1Pole::ota(p4.s + bias);

        float u = (x * drive * (1.f + thermal) - k * fb) * sag;

        // --- transistor input stage ---
        u = OTA_TPT1Pole::ota(u + bias);

        // --- ladder ---
        float y1 = p1.process(u);
        float y2 = p2.process(y1);
        float y3 = p3.process(y2);
        float y4 = p4.process(y3);

        p1.s = zap(p1.s);
        p2.s = zap(p2.s);
        p3.s = zap(p3.s);
        p4.s = zap(p4.s);
        bias = zap(bias);

        return dcBlock(y4);
    }

    // --- Core filter ---
    OTA_TPT1Pole p1, p2, p3, p4;
    AnalogVariation variation;

    // --- Analog behavior ---
    TPT1Pole envLP;       // envelope follower
    TPT1Pole biasLP;      // bias memory (slow)
    float bias = 0.f;

    // --- Parameters ---
    float cutoff = 1000.f;
    float resonance = 0.5f;
    float drive = 1.f;

    float biasAmount = 0.25f;
    float sagAmount  = 0.2f;
    float driftLP = 0.f;

    float k = 0.f;       // feedback amount
    float dc_x1=0, dc_y1=0;
    inline float dcBlock(float x){
        float y = x - dc_x1 + 0.995f * dc_y1;
        dc_x1 = x; dc_y1 = y;
        return y;
    }

};



