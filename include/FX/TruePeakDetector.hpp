#pragma once
#include <algorithm>
#include <cmath>

// Simple polyphase FIR upsampler for true peak detection
template <class Sample>
class TruePeakDetector {
public:
    TruePeakDetector() {
        reset();
    }

    void reset() {
        for (auto& v : mDelay) v = 0;
        mPeak = 0;
    }

    // Process one input sample, return detected true peak (linear)
    Sample process(Sample x) {
        // Shift delay line
        for (int i = 7; i > 0; --i)
            mDelay[i] = mDelay[i - 1];
        mDelay[0] = x;

        // 4× oversampled reconstruction (8-tap lowpass FIR)
        for (int phase = 0; phase < 4; ++phase) {
            Sample y = 0;
            for (int i = 0; i < 8; ++i)
                y += mDelay[i] * fir[phase][i];

            mPeak = std::max(mPeak, std::abs(y));
        }

        return mPeak;
    }

    Sample value() const { return mPeak; }

private:
    Sample mDelay[8]{};
    Sample mPeak{0};

    // 8-tap polyphase lowpass, normalized
    static constexpr Sample fir[4][8] = {
        { -0.0102f,  0.0281f, -0.0811f,  0.5645f,  0.5645f, -0.0811f,  0.0281f, -0.0102f },
        { -0.0013f, -0.0202f,  0.1004f,  0.4216f,  0.4216f,  0.1004f, -0.0202f, -0.0013f },
        {  0.0115f, -0.0383f,  0.1881f,  0.3387f,  0.3387f,  0.1881f, -0.0383f,  0.0115f },
        {  0.0200f, -0.0509f,  0.2621f,  0.2673f,  0.2673f,  0.2621f, -0.0509f,  0.0200f }
    };
};
