#pragma once

class AutoCompressor {
public:
    AutoCompressor();

    void reset();

    void process(float energy,
                 float arousal,
                 float tension,
                 float grooveTightness);

    float threshold() const { return mThreshold; }  // dB
    float ratio()     const { return mRatio; }
    float attack()    const { return mAttack; }     // ms
    float release()   const { return mRelease; }    // ms
    float makeup()    const { return mMakeup; }     // dB

private:
    float mThreshold;
    float mRatio;
    float mAttack;
    float mRelease;
    float mMakeup;
};
