struct BandControls {
    bool mute = false;
    bool solo = false;
    float gainDb = 0.0f;     // per-band trim
};

class BandRouter {
public:
    BandRouter(float sampleRate=48000.0f)
    {
        setSampleRate(sampleRate);
    }

    void setSampleRate(float sr){
        sampleRate = sr;

        // propagate to filters
        x1.lp1.onDomainChange(sr);
        x1.lp2.onDomainChange(sr);
        x1.hp1.onDomainChange(sr);
        x1.hp2.onDomainChange(sr);

        x2.lp1.onDomainChange(sr);
        x2.lp2.onDomainChange(sr);
        x2.hp1.onDomainChange(sr);
        x2.hp2.onDomainChange(sr);

        router.setSampleRate(sr);
    }

    // Call whenever UI changes mute/solo/gain
    void updateTargets()
    {
        const bool anySolo = (c[0].solo || c[1].solo || c[2].solo);

        float sumAbsG = 0.0f;
        bool anyPassing = false;

        for(int i=0;i<3;++i){
            bool pass = true;

            if(anySolo) pass = c[i].solo;
            if(c[i].mute) pass = false;

            float g = pass ? dbToLin(c[i].gainDb) : 0.0f;
            bandGain[i].targetTo(g);

            if(pass){
                anyPassing = true;
                sumAbsG += std::abs(g);
            }
        }

        if(!enableMakeup){
            makeup.targetTo(1.0f);
            return;
        }

        if(!anyPassing){
            makeup.targetTo(0.0f);
            return;
        }

        const float baseline = 3.0f;
        float m = baseline / sumAbsG;
        m = std::clamp(m, 0.0f, 4.0f);  // safety

        makeup.targetTo(m);
    }

    // Apply to band samples and sum
    float process(float low, float mid, float high, float* outLow=nullptr, float* outMid=nullptr, float* outHigh=nullptr)
    {
        float g0 = bandGain[0];
        float g1 = bandGain[1];
        float g2 = bandGain[2];
        float mk = makeup;

        low  *= g0;
        mid  *= g1;
        high *= g2;

        if(outLow)  *outLow  = low;
        if(outMid)  *outMid  = mid;
        if(outHigh) *outHigh = high;

        return (low + mid + high) * mk;
    }

    void forceReset()
    {
        for(int i=0;i<3;++i){
            bandGain[i].reset(bandGain[i].target());
        }
        makeup.reset(makeup.target());
    }

    BandControls c[3];
    bool enableMakeup = true;

private:
    float sr = 48000.0f;
    float bandGain[3];
    float makeup;
};
