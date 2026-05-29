#pragma once
#include "GDSP_ModDelay.hpp"

class ModStereoFlanger : public Function
{
public:
    ModStereoFlanger()
    {
        // Left channel
        L.setDelay(0.002f);     // 2 ms
        L.setFM(0.7f);
        L.setMM(0.5f);
        L.setAM(1.0f);

        // Right channel
        R.setDelay(0.002f);
        R.setFM(0.7f);
        R.setMM(0.5f);
        R.setAM(1.0f);

        setDepth(0.8f);
        setWidth(0.7f);
    }

    // ---------------- Control ----------------
    void setRate(float r)  { lfo.set(r); }
    void setDepth(float d) { depth.set(d); }
    void setWidth(float w){ width = std::clamp(w, 0.f, 1.f); }

    // ---------------- Audio ----------------
    void process(float inL, float inR, float& outL, float& outR)
    {
        float phase = lfo.process();
        float d = std::clamp(depth.process(), 0.f, 1.f);

        float lMod =  phase * d;
        float rMod = -phase * d * width;

        L.setDM(lMod);
        R.setDM(rMod);

        outL = L.update(inL);
        outR = R.update(inR);
    }

private:
    ModDelay L, R;

    Modulator lfo;
    Modulator depth;

    float width = 0.7f;
};
