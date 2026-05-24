#include "GDSP_Delay.hpp"

class SMReverb {
public:
    SMReverb()
    : early(0.1f, 4, 0.01f),
      combs{ Comb(0.1f), Comb(0.1f), Comb(0.1f), Comb(0.1f) },
      diff1(0.02f), diff2(0.02f)
    {
        early.setIpolType(LINEAR);

        const float combTimes[] = {0.0297f, 0.0371f, 0.0411f, 0.0437f};
        for (int i = 0; i < 4; ++i) {
            combs[i].setDelay(combTimes[i]);
            combs[i].setFeedback(0.78f);
        }

        diff1.setDelay(0.005f); diff1.setAllPass(0.7f);
        diff2.setDelay(0.0017f); diff2.setAllPass(0.7f);
    }

    float process(float x)
    {
        float y = early.process(x);

        float tail = 0.f;
        for (auto& c : combs)
            tail += c.process(y);

        tail *= 0.25f;

        tail = diff1.process(tail);
        tail = diff2.process(tail);

        return 0.6f * tail + 0.4f * x;
    }

private:
    MultitapDelay early;
    Comb combs[4];
    Comb diff1, diff2;
};

