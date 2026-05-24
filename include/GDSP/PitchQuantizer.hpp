#pragma once

#include <cmath>    // std::log2, std::pow, std::floor
#include <cfloat>   // (not strictly required here, but often used for numeric limits)

inline double hzToMidi(double hz)
{
    return 69.0 + 12.0 * std::log2(hz / 440.0);
}

inline double midiToHz(double m)
{
    return 440.0 * std::pow(2.0, (m - 69.0) / 12.0);
}

class PitchQuantizer {
public:
    PitchQuantizer(const int* scale, int scaleSize)
        : scale_(scale), scaleSize_(scaleSize) {}

    double quantize(double hz)
    {
        if (hz <= 0.0) return hz;

        double midi = hzToMidi(hz);
        int base = int(std::floor(midi));
        int octave = base / 12;

        double best = 1e9;
        int bestNote = base;

        for (int o = -1; o <= 1; ++o)
        {
            for (int i = 0; i < scaleSize_; ++i)
            {
                int note = (octave + o) * 12 + scale_[i];
                double d = std::abs(note - midi);
                if (d < best)
                {
                    best = d;
                    bestNote = note;
                }
            }
        }

        return midiToHz(bestNote);
    }

private:
    const int* scale_;
    int scaleSize_;
};