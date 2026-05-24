// GDSP_BlockFunction.hpp
template<size_t N>
class BlockFunction : public Function {
public:
    BlockFunction() {
        reset();
    }

    void reset() {
        index = 0;
        output = 0.f;
    }

    float process(float input) override {
        buffer[index++] = input;

        if (index == N) {
            compute(buffer);
            index = 0;
        }

        return output;
    }

protected:
    virtual void compute(float* block) = 0;

    float buffer[N];
    size_t index = 0;
    float output = 0.f;
};

class RMSNode : public BlockFunction<256> {
protected:
    void compute(float* block) override {
        output = gam::arr::rms(block, 256);
    }
};

class ZCRNode : public BlockFunction<256> {
public:
    ZCRNode() : prev(0.f) {}

protected:
    void compute(float* block) override {
        output = float(gam::arr::zeroCross(block, 256, prev)) / 256.f;
        prev = block[255];
    }

private:
    float prev;
};

class MADNode : public BlockFunction<128> {
protected:
    void compute(float* block) override {
        output = gam::arr::meanAbsDiff(block, 128);
    }
};
