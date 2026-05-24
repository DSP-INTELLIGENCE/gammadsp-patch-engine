// GDSP_FunctionN.hpp
// FunctionN
class SumN: public FunctionN
{
public:
    virtual ~SumN() {}
    float process(const float * input, size_t n) {
        return gam::sum(input,n);
    }
};

// FunctionN
class VarianceN: public FunctionN
{
public:
    virtual ~VarianceN() {}
    float process(const float * input, size_t n) {
        return gam::variance(input,n);
    }
};

// FunctionN
class ZeroCountN: public FunctionN
{
public:
    virtual ~ZeroCountN() {}
    float process(const float * input, size_t n) {
        return gam::zeroCount(input,n);
    }
};

class ZeroCrossN : public FunctionN {
public:
    float prev = 0.f;

    float process(const float* input, size_t n) override {
        unsigned zc = gam::zeroCross(input, (unsigned)n, prev);
        prev = zc;
        return float(zc);
    }
};


// FunctionN
class NormN: public FunctionN
{
public:
    virtual ~NormN() {}
    float process(const float * input, size_t n) {
        return gam::norm(input,n);
    }
};


class MeanN: public FunctionN
{
public:
    virtual ~MeanN() {}
    float process(const float * input, size_t n) {
        return gam::mean(input,n);
    }
};

class SumGainN : public FunctionN {
public:
    float process(const float* input, size_t n) override {
        float r = gam::sum(input, n);
        float gain = 1.f / std::sqrt(std::max<size_t>(1, n));
        return r * gain;
    }
};

class MaxN : public FunctionN {
public:
    float process(const float* in, size_t n) override {
        if(!n) return 0.f;
        float m = in[0];
        for(size_t i=1;i<n;i++) m = std::max(m, in[i]);
        return m;
    }
};

class MinN : public FunctionN {
public:
    float process(const float* in, size_t n) override {
        if(!n) return 0.f;
        float m = in[0];
        for(size_t i=1;i<n;i++) m = std::min(m, in[i]);
        return m;
    }
};

class RMSN : public FunctionN {
public:
    float process(const float* in, size_t n) override {
        return gam::rms(in,n);
    }
};

class ProductN : public FunctionN {
public:
    float process(const float* in, size_t n) override {
        float r = 1.f;
        for(size_t i=0;i<n;i++) r *= in[i];
        return r;
    }
};

class WeightedSumN : public FunctionN {
public:
    std::vector<float> weights;

    float process(const float* in, size_t n) override {
        float r = 0.f;
        size_t k = std::min(n, weights.size());
        for(size_t i=0;i<k;i++) r += in[i] * weights[i];
        return r;
    }
};

class SaturatingSumN : public FunctionN {
public:
    float process(const float* in, size_t n) override {
        float r = 0.f;
        for(size_t i=0;i<n;i++) r += in[i];
        return std::tanh(r);
    }
};

class ClampN : public FunctionN {
public:
    float lo = -1.f, hi = 1.f;

    float process(const float* in, size_t n) override {
        float r = 0.f;
        for(size_t i=0;i<n;i++) r += in[i];
        return std::clamp(r, lo, hi);
    }
};


class EnergyN : public FunctionN {
public:
    float process(const float* in, size_t n) override {
        float e = 0.f;
        for(size_t i=0;i<n;i++) e += in[i]*in[i];
        return e;
    }
};

class MedianN : public FunctionN {
public:
    float process(const float* in, size_t n) override {
        temp.assign(in, in+n);
        std::nth_element(temp.begin(), temp.begin()+n/2, temp.end());
        return temp[n/2];
    }
private:
    std::vector<float> temp;
};

class CrestFactorN : public FunctionN {
public:
    float process(const float* in, size_t n) override {
        float peak = 0.f, rms = 0.f;
        for(size_t i=0;i<n;i++){
            peak = std::max(peak, std::fabs(in[i]));
            rms += in[i]*in[i];
        }
        rms = std::sqrt(rms / n);
        return (rms > 0.f) ? peak / rms : 0.f;
    }
};

class SpectralFluxN : public FunctionN {
public:
    std::vector<float> prev;

    float process(const float* in, size_t n) override {
        if(prev.size() != n) prev.assign(n, 0.f);
        float flux = 0.f;
        for(size_t i=0;i<n;i++){
            float d = in[i] - prev[i];
            if(d > 0.f) flux += d;
            prev[i] = in[i];
        }
        return flux;
    }
};

class SkewnessN : public FunctionN {
public:
    float process(const float* in, size_t n) override {
        float mean = 0.f;
        for(size_t i=0;i<n;i++) mean += in[i];
        mean /= n;

        float m2 = 0.f, m3 = 0.f;
        for(size_t i=0;i<n;i++){
            float d = in[i] - mean;
            m2 += d*d;
            m3 += d*d*d;
        }
        m2 /= n; m3 /= n;
        return (m2 > 0.f) ? m3 / std::pow(m2, 1.5f) : 0.f;
    }
};
