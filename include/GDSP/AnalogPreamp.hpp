#include "TPT1Pole.hpp"
#include "AnalogOTAFilter.hpp"

struct AnalogPreamp
{
    float sr = 44100.f;

    // Input conditioning
    TPT1Pole inputHP;
    TPT1Pole inputLP;

    // Core gain stage
    AnalogOTALadder ladder;

    // Output conditioning
    TPT1Pole toneLP;
    TPT1Pole outputHP;

    // Controls
    float gain   = 2.5f;   // preamp drive
    float tone   = 0.5f;   // brightness
    float level  = 0.7f;

    void init(float fs)
    {
        sr = fs;

        inputHP.setHPCut(30.f, sr);     // input coupling cap
        inputLP.setLPCut(12000.f, sr);  // bandwidth limit

        ladder.init(sr);

        toneLP.setLPCut(5000.f, sr);
        outputHP.setHPCut(20.f, sr);

        reset();
    }

    void reset()
    {
        inputHP.reset();
        inputLP.reset();
        ladder.reset();
        toneLP.reset();
        outputHP.reset();
    }

    inline float process(float x)
    {
        // Input conditioning
        x = inputHP.processHP(x);
        x = inputLP.processLP(x);

        // Gain stage
        x = ladder.process(x * gain);

        // Tone control (simple lowpass brightness)
        float toneHz = 800.f + tone * tone * 6000.f;
        toneLP.setLPCut(toneHz, sr);
        x = toneLP.processLP(x);

        // Output cleanup
        x = outputHP.processHP(x);

        return x * level;
    }
};

