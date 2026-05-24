class Spatializer
{
public:
    void setAlgorithm(StereoAlgo a) { algo = a; }

    void setWidth(float w)   { width = std::clamp(w, 0.f, 1.f); }
    void setHaas(float ms)   { haasTime = std::clamp(ms * 0.001f, 0.f, 0.03f); }
    void setTone(float t)    { tone = std::clamp(t, 0.f, 1.f); }
    void setMotion(float m)  { motion = std::clamp(m, 0.f, 1.f); }

    void process(float in, float& L, float& R)
    {
        switch(algo)
        {
        case StereoAlgo::Haas:            haas(in, L, R); break;
        case StereoAlgo::MidSideWidth:    midside(in, L, R); break;
        case StereoAlgo::Weidenfeller:    weidenfeller(in, L, R); break;
        case StereoAlgo::Blumlein:        blumlein(in, L, R); break;
        case StereoAlgo::DualDelay:       dualdelay(in, L, R); break;
        case StereoAlgo::RandomDecorrelate:randomDecor(in, L, R); break;
        default: L = R = in; break;
        }
    }

private:
    StereoAlgo algo = StereoAlgo::None;

    float width  = 0.7f;
    float haasTime = 0.007f;
    float tone = 0.5f;
    float motion = 0.0f;

    Delay haasDelay { 0.05f, 0.0f };

    // ---------------- Algorithms ----------------

    void haas(float in, float& L, float& R)
    {
        haasDelay.write(in);
        L = in;
        R = haasDelay.readAt(haasTime);

        midside(L, R);
    }

    void midside(float in, float& L, float& R)
    {
        float mid  = in;
        float side = in * width;

        L = mid + side;
        R = mid - side;
    }

    void weidenfeller(float in, float& L, float& R)
    {
        float a = 0.5f + width * 0.5f;
        float b = 0.5f - width * 0.5f;

        L = in * a;
        R = in * b;
    }

    void blumlein(float in, float& L, float& R)
    {
        float s = width;
        L = in * (1.f + s);
        R = in * (1.f - s);
    }

    void dualdelay(float in, float& L, float& R)
    {
        float d = haasTime;
        haasDelay.write(in);
        L = haasDelay.readAt(d * 0.5f);
        R = haasDelay.readAt(d);
    }

    void randomDecor(float in, float& L, float& R)
    {
        float n = (rand() / float(RAND_MAX) - 0.5f) * 0.002f;
        haasDelay.write(in);
        L = in;
        R = haasDelay.readAt(haasTime + n);
    }
};

