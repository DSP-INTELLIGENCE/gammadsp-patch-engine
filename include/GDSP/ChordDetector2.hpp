#pragma once
#include <cmath>

struct ChordTemplate {
    int notes[4];
    int count;
    int type;
};

static const ChordTemplate templates[] = {
    {{0,4,7, -1},3, MAJOR},
    {{0,3,7, -1},3, MINOR},
    {{0,3,6, -1},3, DIM},
    {{0,4,8, -1},3, AUG},
    {{0,4,7,10},4, DOM7},
    {{0,2,7, -1},3, SUS2},
    {{0,5,7, -1},3, SUS4},
    {{0,7, -1,-1},2, POWER}
};

class ChordDetector {
public:
    ChordDetector(float sampleRate)
    : mSampleRate(sr), mRoot(-1), mType(NONE), mConfidence(0.0f)
    {
        reset();
    }

    void reset()
    {
        for(int i=0;i<12;i++) mHistogram[i] = 0.0f;
        mRoot = -1;
        mType = NONE;
        mConfidence = 0.0f;
    }

    void process(float pitch, float harmonicity)
    {
        if(pitch <= 0.0f || harmonicity < 0.7f)
            return;

        float midi = 69.0f + 12.0f * log2f(pitch / 440.0f);
        int pc = ((int)std::round(midi)) % 12;
        if(pc < 0) pc += 12;

        mHistogram[pc] += harmonicity * 0.02f;

        evaluate();
    }

    void evaluate()
    {
        float bestScore = 0.0f;
        int bestRoot = -1;
        int bestType = NONE;

        for(int root=0; root<12; root++)
        {
            for(const auto& t : templates)
            {
                float score = 0.0f;

                for(int i=0;i<t.count;i++)
                {
                    int note = (root + t.notes[i]) % 12;
                    score += mHistogram[note];
                }

                if(score > bestScore)
                {
                    bestScore = score;
                    bestRoot = root;
                    bestType = t.type;
                }
            }
        }

        float targetConf = std::min(bestScore * 0.5f, 1.0f);

        mConfidence = 0.9f * mConfidence + 0.1f * targetConf;

        if(mConfidence > 0.5f)
        {
            mRoot = bestRoot;
            mType = bestType;
        }
        else
        {
            mRoot = -1;
            mType = NONE;
        }
    }

    int  root() const { return mRoot; }     // 0..11 (C..B), -1 if none
    int  type() const { return mType; }     // enum below
    float confidence() const { return mConfidence; }

    enum {
        NONE,
        MAJOR,
        MINOR,
        DIM,
        AUG,
        DOM7,
        SUS2,
        SUS4,
        POWER
    };

private:
    float mSampleRate;

    float mHistogram[12];

    int   mRoot;
    int   mType;
    float mConfidence;

    void evaluate();
};
