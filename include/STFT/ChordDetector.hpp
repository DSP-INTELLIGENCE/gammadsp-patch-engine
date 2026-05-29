enum class ChordQuality { Major, Minor, Unknown };

struct ChordResult {
    int root = -1;                 // 0..11 (C..B)
    ChordQuality quality = ChordQuality::Unknown;
    double confidence = 0.0;        // 0..1-ish
};

inline const char* noteName12(int n){
    static const char* names[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    return names[n % 12];
}

inline const char* qualityName(ChordQuality q){
    switch(q){
        case ChordQuality::Major: return "maj";
        case ChordQuality::Minor: return "min";
        default: return "?";
    }
}

class ChromaExtractor {
public:
    explicit ChromaExtractor(double sampleRate, unsigned fftSize)
    : sr_(sampleRate), fftSize_(fftSize) {}

    // bins[k][0] expected = magnitude
    void compute(const gam::Complex<float>* bins, unsigned numBins, float* chroma12) const
    {
        std::fill(chroma12, chroma12 + 12, 0.0f);

        for(unsigned k = 1; k + 1 < numBins; ++k){ // skip DC/Nyquist
            const float mag = bins[k][0];
            if(mag <= 0.0f) continue;

            const double f = (double(k) * sr_) / double(fftSize_);
            if(f < 30.0) continue; // ignore sub-bass rumble

            const double midi = 69.0 + 12.0 * std::log2(f / 440.0);
            const int pc = ((int)std::lround(midi) % 12 + 12) % 12;

            chroma12[pc] += mag;
        }

        // normalize to unit sum (optional but helps scoring stability)
        float sum = 0.0f;
        for(int i=0;i<12;++i) sum += chroma12[i];
        if(sum > 0.0f){
            for(int i=0;i<12;++i) chroma12[i] /= sum;
        }
    }

private:
    double sr_;
    unsigned fftSize_;
};

class ChordDetector {
public:
    ChordDetector(float chromaSmooth = 0.85f)
    : a_(std::clamp(chromaSmooth, 0.0f, 0.999f))
    {
        std::fill(smooth_, smooth_+12, 0.0f);
    }

    ChordResult update(const float* chroma12)
    {
        // smooth chroma
        for(int i=0;i<12;++i){
            smooth_[i] = smooth_[i]*a_ + chroma12[i]*(1.0f - a_);
        }

        // score templates
        double best = -1.0;
        int bestRoot = -1;
        ChordQuality bestQ = ChordQuality::Unknown;

        for(int root=0; root<12; ++root){
            const double maj = scoreTriad_(root, true);
            const double min = scoreTriad_(root, false);

            if(maj > best){ best = maj; bestRoot = root; bestQ = ChordQuality::Major; }
            if(min > best){ best = min; bestRoot = root; bestQ = ChordQuality::Minor; }
        }

        // confidence: compare best vs second best (simple margin)
        double second = -1.0;
        for(int root=0; root<12; ++root){
            const double maj = scoreTriad_(root, true);
            const double min = scoreTriad_(root, false);
            if(root == bestRoot && ((bestQ==ChordQuality::Major && maj==best) || (bestQ==ChordQuality::Minor && min==best))) {
                continue;
            }
            second = std::max(second, std::max(maj, min));
        }

        ChordResult r;
        r.root = bestRoot;
        r.quality = bestQ;
        r.confidence = (best > 0.0) ? (best - second) / std::max(1e-9, best) : 0.0;

        // optional: reject if too uncertain
        if(r.confidence < 0.15){
            r.root = -1;
            r.quality = ChordQuality::Unknown;
        }

        return r;
    }

private:
    // Major triad: 0,4,7; Minor triad: 0,3,7
    double scoreTriad_(int root, bool major) const
    {
        const int third = (root + (major ? 4 : 3)) % 12;
        const int fifth = (root + 7) % 12;

        // Encourage chord tones, slightly discourage non-tones
        const double tones = smooth_[root] + smooth_[third] + smooth_[fifth];

        // A little penalty for energy outside the triad (optional)
        double out = 0.0;
        for(int i=0;i<12;++i){
            if(i==root || i==third || i==fifth) continue;
            out += smooth_[i];
        }

        return tones - 0.35 * out;
    }

private:
    float a_;
    float smooth_[12];
};

/*
## 4) How to use it with your Gamma STFT frame style

In your STFT-based processor, at the frame boundary (or right after `stft.forward(window)`), do:

```cpp
// Assume STFT is in MAG_FREQ or MAG_PHASE; magnitude is bins[k][0] after copyBinsToAux(0,0) like you do.
ChromaExtractor chroma(sr, fftSize);
ChordDetector detector;

float chroma12[12];

void onFrameReady(gam::STFT& stft){
    // If you already keep bins in MAG_FREQ, bins[k][0] is magnitude
    chroma.compute(stft.bins(), stft.numBins(), chroma12);

    ChordResult c = detector.update(chroma12);
    if(c.root >= 0){
        // Example print:
        // printf("%s%s (conf=%.2f)\n", noteName12(c.root), qualityName(c.quality), c.confidence);
    }
}
```

If you’re doing the “aux buffer” pattern (like your style), it also works by using `aux(0)` magnitudes and looping bins to compute frequency from `k`.

---

## Extending to 7ths and suspensions

Add templates:

* Maj7: {0,4,7,11}
* Min7: {0,3,7,10}
* Dom7: {0,4,7,10}
* Sus2: {0,2,7}
* Sus4: {0,5,7}

Then score them the same way (sum tones – penalty out-of-set), pick the best.
*/