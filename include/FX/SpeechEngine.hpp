#pragma once
#include <vector>
#include <cmath>
#include <sndfile.h>


struct Wavetable {
    unsigned frameSize;
    std::vector<std::vector<float>> frames;
};

std::vector<float> loadSerumFile(const std::string& path) {
    SF_INFO info{};
    SNDFILE* file = sf_open(path.c_str(), SFM_READ, &info);
    if(!file) throw std::runtime_error("Failed to open wavetable");

    std::vector<float> data(info.frames);
    sf_readf_float(file, data.data(), info.frames);
    sf_close(file);

    return data;
}

void loadFrame(Osc& osc, const Wavetable& wt, unsigned frameIndex) {
    osc.clearTable();
    for(unsigned i=0;i<wt.frameSize;i++)
        osc.setTableSample(i, wt.frames[frameIndex][i]);
}

void loadMorph(Osc& osc, const Wavetable& wt, float pos){
    unsigned i0 = floor(pos);
    unsigned i1 = std::min(i0+1, (unsigned)wt.frames.size()-1);
    float t = pos - i0;

    osc.clearTable();
    for(unsigned i=0;i<wt.frameSize;i++){
        float v = (1-t)*wt.frames[i0][i] + t*wt.frames[i1][i];
        osc.setTableSample(i, v);
    }
}

class WavetableOsc : public ModGenerator {
public:
    WavetableOsc(float freq = 440.f, unsigned tableSize = 2048)
    : mOsc(freq, 0.f, tableSize) {}

    // ---------- wavetable management ----------

    void loadWavetable(const std::vector<float>& samples) {
        mFrameSize = mOsc.size();
        mFrames = samples.size() / mFrameSize;
        mTable = samples;
        setPosition(0.f);
    }

    void setPosition(float p) {  // 0..1
        p = std::clamp(p, 0.f, 1.f);
        mPos = p * (mFrames - 1);
        updateFrame();
    }

    // ---------- modulation compatible ----------

    float process() override {
        return update(mOsc);
    }

    void setFreq(float f) { baseFreq = f; mOsc.freq(f); }

private:
    void updateFrame() {
        unsigned i0 = (unsigned) mPos;
        unsigned i1 = std::min(i0 + 1, mFrames - 1);
        float t = mPos - i0;

        mOsc.clearTable();

        for(unsigned i = 0; i < mFrameSize; ++i) {
            float a = mTable[i0 * mFrameSize + i];
            float b = mTable[i1 * mFrameSize + i];
            mOsc.setTableSample(i, a + (b - a) * t);
        }
    }

private:
    gam::Osc<double> mOsc;
    std::vector<float> mTable;
    unsigned mFrameSize = 0;
    unsigned mFrames = 0;
    float mPos = 0.f;
};


class VocalTract : public Function {
public:
    using Voice   = gam::Vowel::Voice;
    using Phoneme = gam::Vowel::Phoneme;

    void setCurrentVowel(Voice v, Phoneme p) {
        formant.setCurrentVowel(v, p);
    }

    void setTargetVowel(Voice v, Phoneme p) {
        formant.setTargetVowel(v, p);
    }

    void setMorphSpeed(float s) {
        formant.setMorphSpeed(s);
    }

    float process(float excitation) override {
        float tract = formant.process(excitation);
        return radiation.process(tract);
    }

private:
    FormantMorphFilter formant;
    RadiationFilter    radiation;
};

class SpeechEngine : public Generator {
public:
    SpeechEngine()
    {
        noise.seed(1337);
    }

    void setVoice(gam::Vowel::Voice v) {
        voice = v;
    }

    void setPitch(float f) {
        baseFreq = f;
        source.setFreq(f);
    }

    void setVowel(gam::Vowel::Phoneme p) {
        tract.setTargetVowel(voice, p);
    }

    void triggerConsonant(float amount = 1.0f) {
        burst.trigger();
        consonantLevel = amount;
    }

    float process() override {
        // ─── excitation ───
        float glottal = source.process();
        float breath  = noise.process() * breathAmt;
        float cons    = burst.process() * consonantLevel;

        float excitation = glottal + breath + cons;

        // ─── vocal tract ───
        return tract.process(excitation);
    }

private:
    gam::Vowel::Voice voice = gam::Vowel::MAN;

    // excitation
    ModBuzz    source {110.f};
    NoiseWhite noise;
    Burst      burst;
    float      breathAmt = 0.02f;
    float      consonantLevel = 0.f;

    // articulation
    VocalTract tract;

    float baseFreq = 110.f;
};

struct PhonemeEvent {
    gam::Vowel::Phoneme vowel;
    bool consonant;
    float duration;
};

class PhonemeSequencer {
public:
    void set(const std::vector<PhonemeEvent>& p) {
        seq = p;
        idx = 0;
        time = 0.f;
    }

    bool tick(float dt, PhonemeEvent& out) {
        if (seq.empty()) return false;

        time += dt;
        if (time >= seq[idx].duration) {
            time = 0.f;
            idx = (idx + 1) % seq.size();
            out = seq[idx];
            return true;
        }
        return false;
    }

private:
    std::vector<PhonemeEvent> seq;
    size_t idx = 0;
    float time = 0.f;
};
