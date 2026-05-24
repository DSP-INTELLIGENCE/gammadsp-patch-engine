// Voice.hpp
#pragma once
#include <Gamma/Voices.h>
#include "Engine.hpp"

struct Voice : public gam::Voice<float> {

    Generator* root = nullptr;

    // Core DSP
    float operator()() override {
        return root ? root->process() : 0.f;
    }

    // Called when a note is triggered
    void onAttack() {
        if(root) root->reset();
    }

    // Optional release handling
    void onRelease() {
        // leave empty or forward to envelope, etc.
    }

    // Optional: override done() if you want custom voice ending logic
};

template<unsigned Nvoices = 16>
class Voice16 : public Generator {
public:
    Voice16() {
        mVoices.autoPlay(true);
    }

    // Start a new voice
    void noteOn(Generator* g) {
        mVoices.attack([&](Voice& v){
            v.root = g;
        });
    }

    void noteOffAll() {
        mVoices.releaseAll();
    }

    float process() override {
        return mVoices();
    }

private:
    gam::Voices<Voice, Nvoices> mVoices;
};


