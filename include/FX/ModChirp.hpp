#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif
#include <algorithm>
#include "Chirp.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"

class ModChirp : public Generator
{
public:
    ModChirp(float startFreq = 220.f,
             float endFreq   = 0.f,
             float decay60   = 0.2f)
    : baseStart(startFreq),
      baseEnd(endFreq),
      baseDecay(decay60),
      mChirp(startFreq, endFreq, decay60)
    {
        setAM(1.0f);
    }

    // -------- Base controls --------
    void setStart(float f) { baseStart = f; }
    void setEnd(float f)   { baseEnd   = f; }
    void setDecay(float d) { baseDecay = d; }

    // -------- Modulation depths --------
    void setSM(float v) { sm.set(v); }   // start freq mod
    void setEM(float v) { em.set(v); }   // end freq mod
    void setDM(float v) { dm.set(v); }   // decay mod

    // -------- Output gain --------
    void setAM(float v) { am.set(v); }

    void reset()
    {
        mChirp.reset();
    }

    float process() override
    {
        float _sm = sm.process();
        float _em = em.process();
        float _dm = dm.process();

        float start = baseStart + baseStart * _sm;
        float end   = baseEnd   + baseEnd   * _em;
        float decay = baseDecay + baseDecay * _dm;

        start = std::max(start, 0.1f);
        end   = std::max(end,   0.1f);
        decay = std::max(decay, 0.001f);

        mChirp.freq(start, end);
        mChirp.decay(decay);

        return mChirp() * am.process();
    }

private:
    float baseStart;
    float baseEnd;
    float baseDecay;

    gam::Chirp<double> mChirp;

    Modulator sm;  // start freq mod
    Modulator em;  // end freq mod
    Modulator dm;  // decay mod
    Modulator am;
};

