#pragma once
#include <array>
#include <cmath>
#include <algorithm>

class VowelClassifier {
public:
    using Voice   = gam::Vowel::Voice;
    using Phoneme = gam::Vowel::Phoneme;

    struct Result {
        Phoneme primary   = Phoneme::HEED;
        Phoneme secondary = Phoneme::HAD;
        float   morph     = 0.f;     // 0..1 between primary & secondary
        float   confidence= 0.f;     // 0..1
    };

    VowelClassifier(Voice v = Voice::MAN) : mVoice(v) {}

    void setVoice(Voice v) { mVoice = v; }

    Result classify(float F1, float F2)
    {
        Result r;

        if (F1 < 50 || F2 < 200) return r;

        float bestDist = 1e30f;
        float secondDist = 1e30f;

        for (int p = 0; p < gam::Vowel::NUM_PHONEMES; ++p) {
            float f1 = gam::Vowel::freq(mVoice, (Phoneme)p, 0);
            float f2 = gam::Vowel::freq(mVoice, (Phoneme)p, 1);

            float d = distLog(F1, F2, f1, f2);

            if (d < bestDist) {
                secondDist = bestDist;
                r.secondary = r.primary;
                bestDist = d;
                r.primary = (Phoneme)p;
            }
            else if (d < secondDist) {
                secondDist = d;
                r.secondary = (Phoneme)p;
            }
        }

        // Compute morph + confidence
        float total = bestDist + secondDist + 1e-6f;
        r.morph = bestDist / total;
        r.confidence = std::clamp(1.f - bestDist / (secondDist + 1e-6f), 0.f, 1.f);

        return r;
    }

private:
    Voice mVoice;

    static float distLog(float f1, float f2, float t1, float t2)
    {
        float x = std::log(f1) - std::log(t1);
        float y = std::log(f2) - std::log(t2);
        return x*x + y*y;
    }
};
