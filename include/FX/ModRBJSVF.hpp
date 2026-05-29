#include "GDSP_Filters.hpp"
#include "RBJSVF.hpp"

class ModRBJSVFFilter : public ModFilter
{
public:
    ModRBJSVFFilter()
    {
        baseCutoff = 1000.0f;
        baseRes    = 0.707f;
        setAM(1.0f);

        mFilter.setSampleRate(44100.0f);
        mFilter.setCutoff(baseCutoff);
        mFilter.setQ(baseRes);
    }

    // Pass-through configuration
    void setSampleRate(float sr) { mFilter.setSampleRate(sr); }
    void setType(RBJType t)      { mFilter.setType(t); }
    void setGainDB(float db)     { mFilter.setGainDB(db); }
    void setBandwidth(float bw)  { mFilter.setBandwidth(bw); }
    void setShelfSlope(float s)  { mFilter.setShelfSlope(s); }
    void setAlphaMode(RBJAlphaMode m) { mFilter.setAlphaMode(m); }

    // Base parameters (for modulation)
    void setFreq(float f) { baseCutoff = f; }
    void setRes(float r)  { baseRes = r; }

    void reset() { mFilter.reset(); }

    // ---- Adapter layer expected by ModFilter ----
    void freq(float fc) { mFilter.setCutoff(fc); }
    void res (float q ) { mFilter.setQ(q); }

    float operator()(float x) { return mFilter.process(x); }

    float process(float input) override
    {
        return update(*this, input);
    }

private:
    RBJSVF mFilter;
};
