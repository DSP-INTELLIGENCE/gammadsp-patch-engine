
inline double scoreScale(const double* hist, int root, const int* scale, int scaleSize)
{
    double score = 0.0;
    for (int i = 0; i < scaleSize; ++i)
        score += hist[(root + scale[i]) % 12];
    return score;
}


class NoteHistogram {
public:
    void addMidi(int midiNote)
    {
        if (midiNote < 0) return;
        hist_[midiNote % 12] += 1.0;
    }

    // decay over time so it adapts
    void decay(double amount = 0.995)
    {
        for (double& v : hist_) v *= amount;
    }

    int strongestNote() const
    {
        int best = 0;
        for (int i = 1; i < 12; ++i)
            if (hist_[i] > hist_[best]) best = i;
        return best;
    }

    const double* data() const { return hist_; }

private:
    double hist_[12] = {0};
};


class AdaptiveScale {
public:
    void update(int midiNote)
    {
        histogram_.addMidi(midiNote);
        histogram_.decay();

        double bestScore = -1.0;
        int bestRoot = root_;
        const int* bestScale = scale_;
        int bestSize = scaleSize_;

        for (int r = 0; r < 12; ++r) {
            double major = scoreScale(histogram_.data(), r, SCALE_MAJOR, 7);
            double minor = scoreScale(histogram_.data(), r, SCALE_MINOR, 7);

            if (major > bestScore) {
                bestScore = major;
                bestRoot = r;
                bestScale = SCALE_MAJOR;
                bestSize = 7;
            }
            if (minor > bestScore) {
                bestScore = minor;
                bestRoot = r;
                bestScale = SCALE_MINOR;
                bestSize = 7;
            }
        }

        root_ = bestRoot;
        scale_ = bestScale;
        scaleSize_ = bestSize;
    }

    int root() const { return root_; }
    const int* scale() const { return scale_; }
    int size() const { return scaleSize_; }

private:
    NoteHistogram histogram_;
    int root_ = 0;
    const int* scale_ = SCALE_MAJOR;
    int scaleSize_ = 7;
};

/*
AdaptiveScale adaptive;
MusicalQuantizer tuner;

...

int midi;
double hz = tuner.quantize(rawPitch, midi);

adaptive.update(midi);

// update quantizer if scale changed
tuner.setKey((NoteName)adaptive.root());
tuner.setScale(adaptive.scale(), adaptive.size());

double targetHz = tuner.quantize(rawPitch, midi);
double glideHz  = glide.process(targetHz, frameDt);
*/