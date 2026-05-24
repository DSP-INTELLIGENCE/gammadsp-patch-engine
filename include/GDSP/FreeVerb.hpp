class FreeVerb {
public:
    FreeVerb()
    : combs{
        Comb(0.1f), Comb(0.1f), Comb(0.1f), Comb(0.1f),
        Comb(0.1f), Comb(0.1f), Comb(0.1f), Comb(0.1f)
      },
      aps{
        Comb(0.02f), Comb(0.02f), Comb(0.02f), Comb(0.02f)
      }
    {
        const float combTimes[8] = {
            0.0253061f, 0.0269388f, 0.0289569f, 0.0307483f,
            0.0322449f, 0.0338095f, 0.0353061f, 0.0366667f
        };

        for (int i = 0; i < 8; ++i) {
            combs[i].setDelay(combTimes[i]);
            combs[i].setFeedback(0.84f);
        }

        const float apTimes[4] = {
            0.00510204f, 0.0126077f, 0.0100000f, 0.00773243f
        };

        for (int i = 0; i < 4; ++i) {
            aps[i].setDelay(apTimes[i]);
            aps[i].setAllPass(0.5f);
        }
    }

    void setRoomSize(float size) {
        float fb = 0.28f + size * 0.72f;
        for (auto& c : combs)
            c.setFeedback(fb);
    }

    float process(float x)
    {
        float sum = 0.f;
        for (auto& c : combs)
            sum += c.process(x);

        sum *= 0.125f;

        for (auto& ap : aps)
            sum = ap.process(sum);

        return 0.7f * sum + 0.3f * x;
    }

private:
    Comb combs[8];
    Comb aps[4];
};

