#include "GDSP_Filters.hpp"
#include "SVF2.hpp"

class ModSVF2Filter : public ModFilter
{
public:
    ModSVF2Filter()
    {
        baseCutoff = 1000.0f;
        baseRes    = 0.5f;      // maps nicely to r (≈ Moog)
        setAM(1.0f);

        mFilter.setParams(baseRes, 0.0f, baseCutoff);
    }

    void setSampleRate(float fs) { mFilter.setSampleRate(fs); }

    void setDrive(float d)                { mFilter.setDrive(d); }
    void setAnalogFeel(float a)           { mFilter.setAnalogFeel(a); }
    void setCutoffSaturation(float a)     { mFilter.setCutoffSaturation(a); }
    void enablePitchStabilizer(bool e)    { mFilter.enablePitchStabilizer(e); }
    void setPitchStabilizerAmount(float a){ mFilter.setPitchStabilizerAmount(a); }
    void setFeedback(float kHat)          { mKHat = kHat; }

    void reset() { mFilter.reset(); }

    float process(float input) override;
    {
        // Modulation
        float mCut = cutoff.process();   // [-1..1]
        float mRes = res.process();      // [-1..1]
        float mAmp = am.process();       // [0..1]

        float fc = baseCutoff * (1.0f + mCut);
        float r  = baseRes    * (1.0f + mRes);

        fc = std::max(fc, 5.0f);
        r  = std::max(r, 0.001f);

        mFilter.setParams(r, mKHat, fc);

        return mFilter.process(input) * mAmp;
    }


private:
    SVF2 mFilter;
    float mKHat = 0.0f;   // normalized feedback / resonance
};


