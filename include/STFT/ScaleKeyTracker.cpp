#include "ScaleKeyTracker.hpp"
#include <algorithm>

static const float majorProfile[12] = {
    6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88
};

static const float minorProfile[12] = {
    6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17
};

ScaleKeyTracker::ScaleKeyTracker(float sr)
: mSampleRate(sr), mKey(0), mMinor(false), mConfidence(0.0f)
{
    reset();
}

void ScaleKeyTracker::reset()
{
    for(int i=0;i<12;i++) mHistogram[i]=0.0f;
    mKey = 0;
    mMinor = false;
    mConfidence = 0.0f;
}

void ScaleKeyTracker::process(float pitch, float harmonicity)
{
    if(pitch <= 0.0f || harmonicity < 0.7f)
        return;

    float midi = 69.0f + 12.0f * log2f(pitch / 440.0f);
    int pc = ((int)std::round(midi)) % 12;
    if(pc < 0) pc += 12;

    mHistogram[pc] += harmonicity * 0.01f;

    evaluate();
}

void ScaleKeyTracker::evaluate()
{
    float bestScore = 0.0f;
    int bestKey = 0;
    bool bestMinor = false;

    for(int k=0;k<12;k++)
    {
        float majorScore = 0.0f;
        float minorScore = 0.0f;

        for(int i=0;i<12;i++)
        {
            int idx = (i + k) % 12;
            majorScore += mHistogram[i] * majorProfile[idx];
            minorScore += mHistogram[i] * minorProfile[idx];
        }

        if(majorScore > bestScore)
        {
            bestScore = majorScore;
            bestKey = k;
            bestMinor = false;
        }
        if(minorScore > bestScore)
        {
            bestScore = minorScore;
            bestKey = k;
            bestMinor = true;
        }
    }

    float targetConf = std::min(bestScore * 0.01f, 1.0f);

    mConfidence = 0.95f * mConfidence + 0.05f * targetConf;
    mKey = bestKey;
    mMinor = bestMinor;
}
