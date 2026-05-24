#include "AutoMixer.hpp"
#include <algorithm>
#include <cmath>

AutoMixer::AutoMixer(int layers)
: mNumLayers(layers)
{
    mGain = new float[layers];
    mPan  = new float[layers];
    reset();
}

void AutoMixer::reset()
{
    for(int i=0;i<mNumLayers;i++)
    {
        mGain[i] = 0.8f;
        mPan[i]  = (float)i / (float)(mNumLayers - 1) * 2.0f - 1.0f;
    }
}

void AutoMixer::process(float density,
                        float brightness,
                        int   section,
                        float energy)
{
    for(int i=0;i<mNumLayers;i++)
    {
        float role = (float)i / (float)mNumLayers;

        float g = density * (1.0f - role) + 0.4f;
        g *= (0.6f + energy * 0.4f);

        mGain[i] = 0.9f * mGain[i] + 0.1f * std::clamp(g, 0.0f, 1.0f);

        float width = 0.5f + brightness * 0.5f;
        mPan[i] = std::clamp(mPan[i] * width, -1.0f, 1.0f);
    }
}
