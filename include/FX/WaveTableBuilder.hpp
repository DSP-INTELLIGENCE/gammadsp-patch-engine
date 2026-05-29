#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include <Gamma/Vowel.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct WaveTable {
    std::vector<float> data;
    unsigned size;

    explicit WaveTable(unsigned s = 2048) : data(s, 0.f), size(s) {}

    float& operator[](unsigned i) { return data[i]; }
    const float& operator[](unsigned i) const { return data[i]; }
};

struct ConsonantEnvelope {
    float value = 0.f;
    float attack = 0.002f;
    float decay  = 0.04f;
    bool active  = false;

    void trigger() {
        value = 0.f;
        active = true;
    }

    float process(float sr) {
        if(!active) return 0.f;

        float aStep = 1.f / (attack * sr);
        float dStep = 1.f / (decay  * sr);

        if(value < 1.f) value += aStep;
        else value -= dStep;

        if(value <= 0.f) {
            value = 0.f;
            active = false;
        }
        return value;
    }
};

class WaveTableBuilder {
public:
    // ===== Basic =====
    static WaveTable Sine(unsigned size);
    static WaveTable Noise(unsigned size);

    // ===== Band-limited =====
    static WaveTable Saw(unsigned size, float f0, float sr);
    static WaveTable Square(unsigned size, float f0, float sr, float pw = 0.5f);
    static WaveTable Triangle(unsigned size, float f0, float sr);

    // ===== Formant / Vocal =====
    static WaveTable Vowel(unsigned size, float f0, float sr,
                           gam::Vowel::Voice voice,
                           gam::Vowel::Phoneme phoneme);

    static WaveTable VowelMorph(unsigned size, float f0, float sr,
                                gam::Vowel::Voice voice,
                                const std::vector<gam::Vowel::Phoneme>& vowels,
                                float position);

    static WaveTable Consonant(unsigned size, float sr,
                               gam::Vowel::Voice voice,
                               gam::Vowel::Phoneme shape,
                               float brightness = 1.0f);

    // ===== Utilities =====
    static WaveTable Morph(const WaveTable& A, const WaveTable& B, float t);
    static void Normalize(WaveTable& t);
};
