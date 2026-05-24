class HarmonyGenerator {
public:
    // intervals expressed as scale steps (not semitones)
    // Example: {0, 2, 4} → root, 3rd, 5th
    void setVoices(const int* steps, int count)
    {
        steps_.assign(steps, steps + count);
    }

    void generate(int baseMidi,
                  const int* scale, int scaleSize,
                  int rootKey,
                  std::vector<int>& outMidi)
    {
        outMidi.clear();

        if (baseMidi < 0) return;

        // find scale degree of base note
        int pitchClass = (baseMidi - rootKey + 120) % 12;

        int degree = -1;
        for (int i = 0; i < scaleSize; ++i)
            if (scale[i] == pitchClass)
                degree = i;

        if (degree < 0) return;

        int octave = baseMidi / 12;

        for (int step : steps_) {
            int newDegree = degree + step;
            int oShift = newDegree / scaleSize;
            int d = newDegree % scaleSize;

            int midi = (octave + oShift) * 12 + rootKey + scale[d];
            outMidi.push_back(midi);
        }
    }

private:
    std::vector<int> steps_;
};

/*
# 🎹 Example: classic triad

static const int TRIAD[3] = {0, 2, 4}; // root, 3rd, 5th

HarmonyGenerator harmony;
harmony.setVoices(TRIAD, 3);

# 🧠 Plug into your pipeline


int midi;
double tunedHz = tuner.quantize(rawHz, midi);

adaptive.update(midi);

tuner.setKey((NoteName)adaptive.root());
tuner.setScale(adaptive.scale(), adaptive.size());

double glideHz = glide.process(tunedHz, frameDt);

// Generate harmony
std::vector<int> harmonyNotes;
harmony.generate(midi,
                 adaptive.scale(), adaptive.size(),
                 adaptive.root(),
                 harmonyNotes);

Convert harmony notes to Hz:


for (int note : harmonyNotes) {
    double hz = 440.0 * pow(2.0, (note - 69) / 12.0);
}
*/