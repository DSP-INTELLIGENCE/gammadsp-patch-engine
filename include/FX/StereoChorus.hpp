#pragma once
#include "GDSP_ModDelay.hpp"

class ModStereoChorus : public Function
{
public:
    ModStereoChorus()
    {
        // Left
        L.setDelay(0.020f);
        L.setFM(0.0f);
        L.setMM(0.6f);
        L.setAM(1.0f);

        // Right
        R.setDelay(0.020f);
        R.setFM(0.0f);
        R.setMM(0.6f);
        R.setAM(1.0f);

        setDepth(0.002f);
        setWidth(0.5f);
    }

    // ---------------- Control ----------------
    void setRate(float r)   { lfo.set(r); }
    void setDepth(float d)  { depth.set(d); }
    void setWidth(float w)  { width = std::clamp(w, 0.f, 1.f); }

    // ---------------- Audio ----------------
    void process(float inL, float inR, float& outL, float& outR)
    {
        float phase = lfo.process();
        float d = depth.process();

        // phase offset for stereo
        float lMod =  phase;
        float rMod = -phase * width;

        L.setDM(lMod * d);
        R.setDM(rMod * d);

        outL = L.update(inL);
        outR = R.update(inR);
    }

private:
    ModDelay L, R;

    Modulator lfo;
    Modulator depth;

    float width = 0.5f;
};
