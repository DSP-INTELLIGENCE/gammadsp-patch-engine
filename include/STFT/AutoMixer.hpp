#pragma once

class AutoMixer {
public:
    AutoMixer(int numLayers);

    void reset();

    void process(float density,
                 float brightness,
                 int   section,
                 float energy);

    float gain(int layer) const;
    float pan(int layer)  const;   // -1..1

private:
    int mNumLayers;

    float* mGain;
    float* mPan;
};
