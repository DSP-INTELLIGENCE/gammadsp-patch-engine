#include "GDSP_Oscillators.hpp"

struct Grain {
    bool active = false;

    Sweep position;
    Sweep envelope;

    float duration = 0.1f;
    float amplitude = 1.0f;
    float startPhase = 0.0f;

    void trigger(float pos, float dur, float amp) {
        active = true;
        duration = dur;
        amplitude = amp;
        startPhase = pos;

        position.setPeriod(dur);
        position.setPhase(pos);
        envelope.setPeriod(dur);
        envelope.reset();
    }

    float process(const Wavetable& source) {
        if (!active) return 0.0f;

        float p = position.process();
        float env = envelope.process();

        if (env >= 1.0f) {
            active = false;
            return 0.0f;
        }

        float sample = source.sample(p);
        float window = std::sin(M_PI * env);   // Hann window
        return sample * window * amplitude;
    }
};

class Granular {
    public:
        Granular(Wavetable* source, int maxGrains = 32)
            : mSource(source), mGrains(maxGrains)
        {}
    
        void setDensity(float grainsPerSec) {
            mClock.setFreq(grainsPerSec);
        }
    
        void setDuration(float sec) { mDuration = sec; }
        void setAmplitude(float amp) { mAmplitude = amp; }
    
        float process() {
            bool tick = mClock.process();
    
            if (tick) spawnGrain();
    
            float out = 0.0f;
            for (auto& g : mGrains) {
                out += g.process(*mSource);
            }
            return out;
        }
    
    private:
        void spawnGrain() {
            for (auto& g : mGrains) {
                if (!g.active) {
                    float pos = randomFloat();        // 0..1
                    g.trigger(pos, mDuration, mAmplitude);
                    break;
                }
            }
        }
    
    private:
        Wavetable* mSource;
        Clock mClock{10.0f};        // grains per second
        std::vector<Grain> mGrains;
    
        float mDuration = 0.1f;
        float mAmplitude = 0.3f;
    };

/*
Granular gran(&table);

gran.setDensity(25.0f);
gran.setDuration(0.08f);
gran.setAmplitude(0.2f);

for (...) {
    float out = gran.process();
    buffer[i] = out;
}
*/    