#pragma once
#include "GDSP_ModDelay.hpp"

class ModStereoVibrato : public Function
{
public:
    ModStereoVibrato()
    {
        // Left
        L.setDelay(0.005f);     // 5 ms
        L.setFM(0.0f);
        L.setMM(1.0f);         // 100% wet
        L.setAM(1.0f);

        // Right
        R.setDelay(0.005f);
        R.setFM(0.0f);
        R.setMM(1.0f);
        R.setAM(1.0f);

        setDepth(0.6f);
        setWidth(0.7f);
    }

    // ---------- Controls ----------
    void setRate(float r)  { lfo.set(r); }
    void setDepth(float d) { depth.set(d); }
    void setWidth(float w){ width = std::clamp(w, 0.f, 1.f); }

    // ---------- Audio ----------
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
