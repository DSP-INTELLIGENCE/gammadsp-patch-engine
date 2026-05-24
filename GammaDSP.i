%module GammaDSP

%{
#define INC_ALL 1
#include "GammaDSP.hpp"
#include "GDSP/Phase90.hpp"
#include "GDSP/AP1Phaser.hpp"
#include "GDSP/AP2Phaser.hpp"
%}


namespace gam {
    /// Filter types
    enum FilterType{
        LOW_PASS,			/**< Low-pass */
        HIGH_PASS,			/**< High-pass */
        BAND_PASS,			/**< Band-pass */
        RESONANT,			/**< Resonant band-pass */
        BAND_REJECT,		/**< Band-reject */
        ALL_PASS,			/**< All-pass */
        PEAKING,			/**< Peaking */
        LOW_SHELF,			/**< Low-shelf */
        HIGH_SHELF,			/**< High-shelf */
        SMOOTHING,			/**< Smoothing */
        THRU_PASS			/**< Thru-pass (no filter) */
    };

    /// Window types
    enum WindowType{
        BARTLETT,			/**< Bartlett (Triangle) */
        BLACKMAN,			/**< Blackman */
        BLACKMAN_HARRIS,	/**< Blackman-Harris */
        BLACKMAN_NUTTALL,	/**< Blackman-Nuttall */
        FLATTOP,			/**< Flat-Top */
        HAMMING,			/**< Hamming */
        HANN,				/**< von Hann */
        NUTTALL,			/**< Nuttall */
        WELCH,				/**< Welch */
        NYQUIST,			/**< Nyquist */
        RECTANGLE			/**< Rectangle (no window) */
    };


    /// Waveform types
    enum WaveformType{
        SINE,				/**< Sine wave */
        COSINE,				/**< Cosine wave */
        TRIANGLE,			/**< Triangle wave */
        PARABOLIC,			/**< Parabolic wave */
        SQUARE,				/**< Square wave */
        SAW,				/**< Saw wave */
        IMPULSE				/**< Impulse wave */
    };    
}

/// Interpolation types
namespace gam::ipl
{
    enum Type{
        TRUNC=0,	/**< Truncating interpolation */
        ROUND,		/**< Nearest neighbor interpolation */
        MEAN2,		/**< Mean of two nearest neighbors */
        LINEAR,		/**< Linear interpolation */
        CUBIC,		/**< Cubic interpolation */
        ALLPASS		/**< Allpass interpolation */
    };
}


%include "stdint.i"
%include "gamma_typemaps.i"
%apply unsigned int { uint32_t };
%apply int { int32_t };
%include "GammaDSP.hpp"

%inline %{

void run(Generator * p, float * output, size_t n) {
    for(size_t i = 0; i < n; i++) output[i] = p->process();
}

void run(Function * p, const float * input, float * output, size_t n) {
    for(size_t i = 0; i < n; i++) output[i] = p->process(input[i]);
}
%}

%inline %{

void runGenerator(Generator* p, float* output, int n)
{
    if (!p || !output || n <= 0) return;

    for (int i = 0; i < n; ++i)
        output[i] = p->process();
}

void runFunction(Function* p, const float* input, float* output, int n)
{
    if (!p || !input || !output || n <= 0) return;

    for (int i = 0; i < n; ++i)
        output[i] = p->process(input[i]);
}

void runFunctionInPlace(Function* p, float* buffer, int n)
{
    if (!p || !buffer || n <= 0) return;

    for (int i = 0; i < n; ++i)
        buffer[i] = p->process(buffer[i]);
}

%}


%include "GDSP/Phase90.hpp"
%include "GDSP/AP1Phaser.hpp"
%include "GDSP/AP2Phaser.hpp"