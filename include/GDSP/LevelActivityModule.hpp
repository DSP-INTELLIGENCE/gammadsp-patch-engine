// LevelActivityModule.hpp
#include "GammaDSP.hpp"
#include "GDSP_Analysis.hpp"

class LevelActivityModule {
public:
    struct State {
        float level;     // smooth loudness
        float peak;      // true peak envelope
        bool  active;    // sound currently present
        bool  silent;    // stable silence detected
    };

    
    // 🔧 Constructor Wiring
    LevelActivityModule(float sr)
    : env_(sr, 20.0f),
    peak_(512),
    activeThresh_(0.02f, 10.0f),
    silence_(unsigned(sr * 0.05f)),   // 50 ms
    controlClock_(unsigned(sr * 0.01f)), // 10 ms
    sampleRate_(sr)
    {
    }

    // 🎛 Parameter Control

    void setActiveThreshold(float t) {
        activeThresh_.setThreshold(t);
    }

    void setSilenceThreshold(float t) {
        silenceThreshold_ = t;
    }

    void setSilenceTime(float seconds) {
        silence_.setCount(unsigned(seconds * sampleRate_));
    }

    // 🔊 Per-Sample Processing (Audio Thread)

    void process(float x)
    {
        if(!std::isfinite(x)) x = 0.0f;

        state_.level = env_.process(x);
        state_.peak  = peak_.process(x);
    }

    // 🧠 Control-Rate Update (Every 10 ms)

    void update()
    {
        if(!controlClock_.process())
            return;

        state_.active = activeThresh_.process(state_.level) > 0.5f;
        state_.silent = silence_.process(state_.level, silenceThreshold_);
    }

    // ♻ Reset

    void reset()
    {
        env_.reset();
        peak_.reset();
        activeThresh_.reset(0.0f);
        silence_.reset();
        controlClock_.reset();
        state_ = {};
    }

    const State& state() const { return state_; }

private:
    // --- primitives ---
    EnvFollow     env_;
    MaxAbs        peak_;
    Threshold     activeThresh_;
    SilenceDetect silence_;
    PCounter      controlClock_;

    // --- parameters ---
    float sampleRate_;
    float silenceThreshold_{0.001f};

    // --- state ---
    State state_{};
};

