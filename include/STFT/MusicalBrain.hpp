#pragma once
#include <algorithm>

// --- Perception ---
#include "EnergyTracker.hpp"
#include "OnsetDetector.hpp"
#include "BeatTracker.hpp"
#include "PitchStabilityMeter.hpp"
#include "SpectralCentroidDetector.hpp"
#include "SpectralFluxDetector.hpp"
#include "BrightnessEstimator.hpp"

// --- Intelligence ---
#include "ContextAnalyzer.hpp"
#include "PatternDetector.hpp"
#include "HistoryBuffer.hpp"
#include "AdaptiveThreshold.hpp"
#include "SignalProfiler.hpp"

// --- Decision & Control ---
#include "StateMachine.hpp"
#include "BehaviorModel.hpp"
#include "AdaptiveController.hpp"

template <class Sample>
class MusicalBrain {
public:
    struct State {
        // Perception
        Sample energy;
        Sample brightness;
        Sample flux;
        Sample centroid;
        Sample stability;
        bool   onset;
        bool   beat;

        // Intelligence
        typename ContextAnalyzer<Sample>::Context context;
        typename SignalProfiler<Sample>::Profile  profile;
        typename PatternDetector<Sample>::PatternState pattern;

        // Behavior
        typename BehaviorModel<Sample>::Behavior  mood;
    };

    MusicalBrain()
    {
        reset();
    }

    void reset()
    {
        energy.reset();
        onset.reset();
        beatTracker.reset();
        stability.reset();
        centroid.reset();
        flux.reset();
        brightness.reset();
        context.reset();
        profiler.reset();
        pattern.reset();
    }

    // Audio thread
    void processSample(Sample x)
    {
        s.energy     = energy.process(x);
        s.onset      = onset.process(x);
        s.stability  = stability.process(lastPitch);
        s.centroid   = centroid.process(x);
        s.flux       = flux.process(x);
        brightness.processSample(x);

        beatTracker.processSample(x);

        if (s.onset)
            pattern.processOnset();
    }

    // Control thread (block or control rate)
    void update(Sample pitchHz)
    {
        lastPitch = pitchHz;

        beatTracker.update();
        s.beat = beatTracker.state().beat;

        s.brightness = brightness.state().brightness;

        s.context = context.process(
            s.energy, s.flux, s.brightness, s.stability);

        s.pattern = pattern.process();

        s.profile = profiler.process(
            s.energy, s.brightness, s.flux, s.stability);

        s.mood = behavior.process();
    }

    const State& state() const { return s; }

    // Expose key systems for configuration
    BehaviorModel<Sample>& behaviors() { return behavior; }

private:
    // Perception
    EnergyTracker<Sample>          energy;
    OnsetDetector<Sample>          onset;
    BeatTracker<Sample>            beatTracker;
    PitchStabilityMeter<Sample>    stability;
    SpectralCentroidDetector<Sample> centroid;
    SpectralFluxDetector<Sample>     flux;
    BrightnessEstimator<Sample>      brightness;

    // Intelligence
    ContextAnalyzer<Sample>        context;
    PatternDetector<Sample>        pattern;
    HistoryBuffer<Sample>          history;
    AdaptiveThreshold<Sample>      adaptiveThresh;
    SignalProfiler<Sample>         profiler;

    // Behavior
    BehaviorModel<Sample>          behavior;

    // State
    State  s{};
    Sample lastPitch = (Sample)0;
};
