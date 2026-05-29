#include "GDSP_Engine.hpp"
#include "GDSP_Paramters.hpp"

struct ModSource {
    virtual float process() = 0;
    virtual ~ModSource() = default;
};

struct ModTarget {
    virtual void addMod(float v) = 0;
    virtual ~ModTarget() = default;
};

struct ModParam : public ModTarget {
    Modulator* param = nullptr;

    explicit ModParam(Modulator& p) : param(&p) {}

    void addMod(float v) override {
        param->modIn += v;
    }
};

struct ModConnection {
    ModSource* source;
    ModTarget* target;
    float depth;
};

class ModMatrix {
public:
    void connect(ModSource& src, ModTarget& dst, float depth)
    {
        connections.push_back({ &src, &dst, depth });
    }

    void process()
    {
        // 1) Clear previous frame's modulation
        for (auto* p : params)
            p->param->modIn = 0.f;

        // 2) Accumulate this frame's modulation
        for (auto& c : connections) {
            float v = c.source->process() * c.depth;
            c.target->addMod(v);
        }
    }


    void registerParam(ModParam& p)
    {
        params.push_back(&p);
    }

private:
    std::vector<ModConnection> connections;
    std::vector<ModParam*> params;
};
