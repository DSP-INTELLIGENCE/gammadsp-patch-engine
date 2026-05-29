class FDN4 {
public:
    FDN4()
    : d{ Delay(0.2f, 0.031f), Delay(0.2f, 0.037f), Delay(0.2f, 0.041f), Delay(0.2f, 0.043f) }
    {}

    void setFeedback(float g) { fb = std::clamp(g, -0.999f, 0.999f); }

    void setDamping(float dampHz) {
        for (auto& s : sh) s.setDampHz(dampHz);
    }

    void setBassCut(float hz, float amount = 1.0f) {
        for (auto& s : sh) { s.setBassCutHz(hz); s.setBassAmount(amount); }
    }

    void setDrive(float d) {
        for (auto& s : sh) s.setDrive(d);
    }

    float process(float x) {
        // read
        float y0 = d[0].read();
        float y1 = d[1].read();
        float y2 = d[2].read();
        float y3 = d[3].read();

        // shape inside loop (THIS is where RT60 becomes frequency-dependent)
        y0 = sh[0].process(y0);
        y1 = sh[1].process(y1);
        y2 = sh[2].process(y2);
        y3 = sh[3].process(y3);

        // Hadamard mix (energy-preserving with norm=0.5)
        float u0 = ( y0 + y1 + y2 + y3);
        float u1 = ( y0 - y1 + y2 - y3);
        float u2 = ( y0 + y1 - y2 - y3);
        float u3 = ( y0 - y1 - y2 + y3);

        constexpr float norm = 0.5f; // 1/sqrt(4)
        u0 *= norm; u1 *= norm; u2 *= norm; u3 *= norm;

        // write back with global fb
        d[0].write(x + fb * u0);
        d[1].write(x + fb * u1);
        d[2].write(x + fb * u2);
        d[3].write(x + fb * u3);

        // output (sum, or map to stereo later)
        return 0.25f * (y0 + y1 + y2 + y3);
    }

private:
    Delay d[4];
    FeedbackShaper sh[4];
    float fb = 0.75f;
};
