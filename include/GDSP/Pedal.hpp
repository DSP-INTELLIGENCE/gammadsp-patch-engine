#pragma once
#include "Distortion/LUTWaveShaper.hpp"
#include "Distortion/Oversampled.hpp"
#include "Distortion/ToneStack.hpp"
#include "Distortion/TonePresence.hpp"
#include "Distortion/PowerSag.hpp"
#include "Distortion/TubeFeel.hpp"

class Pedal : public Function
{
public:
    Pedal()
    : shaper(),
      os(&shaper),
      env(10.0f)
    {
        env.reset();
    }

    // ----- Controls -----
    void setDrive(float v)      { baseDrive = v; }
    void setBias(float v)       { baseBias  = v; }
    void setAsym(float v)       { baseAsym  = v; }
    void setOutput(float v)     { baseOut   = v; }

    void setTubeFeel(float v)   { tubeFeelAmt = v; }
    void setSag(float v)        { sagAmt = v; }

    void setType(LUTTransferFunction::TransferType t)
    { shaper.setType(t); }
    

    float process(float x) override
    {        
        float drive = baseDrive;
        float bias  = baseBias;
        float asym  = baseAsym;
        float out   = baseOut;

        float e = env.process(std::fabs(x));
        
        tubeFeel.setAmount(tubeFeelAmt);
        powerSag.setAmount(sagAmt);

        tubeFeel.apply(e, drive, bias, asym);
        powerSag.apply(e, drive, out);

        drive = std::clamp(drive, 0.0f, 20.0f);
        bias  = std::clamp(bias, -0.5f, 0.5f);
        asym  = std::clamp(asym, 0.5f, 2.0f);
        out   = std::clamp(out, 0.0f, 2.0f);

        shaper.setDrive(drive);
        shaper.setBias(bias);
        shaper.setAsym(asym);
        shaper.setOutput(out);

        float y = os.process(x);        
        y = shaper.curveColor(y);
        return tonePresence.process(y);        
    }
    void setMacroDrive(float v)
    {
        float newVal = std::clamp(v, 0.0f, 10.0f);
        if(newVal != macroDrive)
        {
            macroDrive = newVal;
            macroDirty = true;
        }
    }


    void applyMacroDrive()
    {
        // normalized 0..1
        float d = macroDrive / 10.0f;

        // ---- Pre-gain ----
        baseDrive = 1.0f + d * 9.0f;

        // ---- Diode behavior ----
        auto& t = shaper.getTransfer();

        t.setDrive(2.0f + d * 18.0f);               // diode drive
        t.setThreshold(0.35f - d * 0.25f);         // knee tightens
        t.setAsym(1.0f + d * 0.8f, 1.0f + d * 1.2f);

        // ---- Tube behavior ----
        t.setTubeABC(0.20f + d * 0.10f,
                    0.04f + d * 0.05f,
                    2.5f  + d * 5.0f);

        // ---- Output trim ----
        baseOut = 1.0f - d * 0.4f;
    }

    LUTWaveShaper& getWaveShaper() { return shaper; }
    TonePresence& getPresence() { return tonePresence; }
    TubeFeel&     getTubeFeel() { return tubeFeel; }
    PowerSag&     getPowerSag() { return powerSag; }

private:
    LUTWaveShaper shaper;
    Oversampled os;

    EnvFollow env;
    TonePresence tonePresence;
    TubeFeel tubeFeel;
    PowerSag powerSag;

    float baseDrive = 1.0f;
    float baseBias  = 0.0f;
    float baseAsym  = 1.0f;
    float baseOut   = 1.0f;

    float tubeFeelAmt = 0.4f;
    float sagAmt = 0.3f;
    float macroDrive = 1.0f;
    bool macroDirty = true;

};
