#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include "STFT.hpp"
#include "RFFT.hpp"
#include "CFFT.hpp"
#include "ActivityAnalyzer.hpp"
#include "PitchDetectorV2.hpp"
#include "OnsetDetector.hpp"
#include "ChordDetector.hpp"

class Analyzer {
public:
    virtual ~Analyzer() = default;
};

class SampleAnalyzer : public Analyzer {
public:
    virtual void processSample(float x) = 0;
};

class FrameAnalyzer : public Analyzer {
public:
    virtual void processFrame(const float* frame) = 0;
};

class SpectralAnalyzer : public Analyzer {
public:
    virtual void processSpectrum(const gam::Complex<float>* bins,
                                 unsigned numBins,
                                 double sampleRate) = 0;
};


class AnalysisEngine {
public:
    explicit AnalysisEngine(float sampleRate)
    : sampleRate_(sampleRate),
      activity_(sampleRate)
    {}

    // ---------------- Registration ----------------

    void setActivityAnalyzer(ActivityAnalyzer* a) {
        activity_ = *a;
    }

    void addSampleAnalyzer(SampleAnalyzer* a) {
        sampleAnalyzers_.push_back(a);
    }

    void addFrameAnalyzer(FrameAnalyzer* a) {
        frameAnalyzers_.push_back(a);
    }

    void addSpectralAnalyzer(SpectralAnalyzer* a) {
        spectralAnalyzers_.push_back(a);
    }

    // ---------------- Runtime ---------------------

    // Call for every sample
    inline void processSample(float x)
    {
        activity_.process(x);

        if (!activity_.isActive())
            return;

        for (auto* a : sampleAnalyzers_)
            a->processSample(x);
    }

    // Call when a time-domain frame is ready
    inline void processFrame(const float* frame)
    {
        if (!activity_.isActive())
            return;

        for (auto* a : frameAnalyzers_)
            a->processFrame(frame);
    }

    // Call when a spectral frame is ready
    inline void processSpectrum(const gam::Complex<float>* bins,
                                unsigned numBins)
    {
        if (!activity_.isActive())
            return;

        for (auto* a : spectralAnalyzers_)
            a->processSpectrum(bins, numBins, sampleRate_);
    }

    // ---------------- Query -----------------------

    bool  isActive() const      { return activity_.isActive(); }
    float activityLevel() const { return activity_.activityLevel(); }

    void reset() {
        activity_.reset();
    }

private:
    float sampleRate_;

    ActivityAnalyzer activity_;

    std::vector<SampleAnalyzer*>   sampleAnalyzers_;
    std::vector<FrameAnalyzer*>    frameAnalyzers_;
    std::vector<SpectralAnalyzer*> spectralAnalyzers_;
};

class PitchAnalyzer : public FrameAnalyzer {
public:
    PitchAnalyzer(size_t size)
    : detector_(size) {}

    void processFrame(const float* frame) override {
        result_ = detector_.analyze(frame);
    }

    const PDResult& result() const { return result_; }

private:
    PitchDetector detector_;
    PDResult result_;
};

class OnsetAnalyzer : public SpectralAnalyzer {
public:
    void processSpectrum(const gam::Complex<float>* bins,
                         unsigned numBins,
                         double) override
    {
        // already stored via STFT internally
        // trigger events here
    }
};

class ChordAnalyzer : public SpectralAnalyzer {
public:
    ChordAnalyzer(double sr, unsigned fft)
    : chroma_(sr, fft) {}

    void processSpectrum(const gam::Complex<float>* bins,
                         unsigned numBins,
                         double sr) override
    {
        chroma_.compute(bins, numBins, chroma12_);
        last_ = detector_.update(chroma12_);
    }

    const ChordResult& result() const { return last_; }

private:
    ChromaExtractor chroma_;
    ChordDetector detector_;
    float chroma12_[12] {};
    ChordResult last_;
};

/*
AnalysisEngine analysis(sr);

analysis.addFrameAnalyzer(&pitchAnalyzer);
analysis.addSpectralAnalyzer(&chordAnalyzer);

// audio callback
for (...) {
    float x = input[i];
    analysis.processSample(x);
    output[i] = x;
}

// when hop/frame ready
analysis.processFrame(frame);

// when STFT ready
analysis.processSpectrum(stft.bins(), stft.numBins());
*/

