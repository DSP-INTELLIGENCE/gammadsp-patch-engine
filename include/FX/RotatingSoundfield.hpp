class RotatingSoundfield : public Function {
public:
    RotatingSoundfield()
    {
        for(int i=0;i<6;i++)
            phaseNet.emplace_back(800.f + i * 250.f);
    }

    void setRate(float r) { rate = r; }

    void process(float& L, float& R)
    {
        float p = phase;
        phase += 2.f * M_PI * rate / gam::sampleRate();

        float c = std::cos(p);
        float s = std::sin(p);

        float xL = L;
        float xR = R;

        // Quadrature rotation
        float qL = xL * c - xR * s;
        float qR = xL * s + xR * c;

        for(auto& a : phaseNet)
        {
            qL = a.process(qL);
            qR = a.process(qR);
        }

        L = qL;
        R = qR;
    }

private:
    std::vector<AllPass1> phaseNet;
    float rate = 0.2f;
    float phase = 0.f;
};
