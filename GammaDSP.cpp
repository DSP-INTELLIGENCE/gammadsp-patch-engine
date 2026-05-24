#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#include <algorithm>
#include <cmath>
#include <random>


extern "C" {
void set_samplerate(double sampleRate) {
    gam::sampleRate(sampleRate);    
}
double get_samplerate() {
   return gam::sampleRate();    
}

void deinterleave(float* interleaved,
                         float* left,
                         float* right,
                         int frames)
{
    gam::Slice<float> src(interleaved, frames, 2, 0);
    gam::Slice<float> dstL(left,  frames);
    gam::Slice<float> dstR(right, frames);

    dstL.copy(src);
    dstR.copy(gam::slice(interleaved, frames, 2, 1));
}

void interleave(float* left,
                       float* right,
                       float* interleaved,
                       int frames)
{
    gam::Slice<float> dst(interleaved, frames, 2, 0);
    gam::Slice<float> srcL(left,  frames);
    gam::Slice<float> srcR(right, frames);

    dst.copy(srcL);
    gam::slice(interleaved, frames, 2, 1).copy(srcR);
}


void deinterleaveN(float* interleaved,
                          float** planar,
                          int channels,
                          int frames)
{
    for (int ch = 0; ch < channels; ++ch)
        gam::slice(planar[ch], frames)
            .copy(gam::slice(interleaved, frames, channels, ch));
}

void interleaveN(float** planar,
                        float* interleaved,
                        int channels,
                        int frames)
{
    for (int ch = 0; ch < channels; ++ch)
        gam::slice(interleaved, frames, channels, ch)
            .copy(gam::slice(planar[ch], frames));
}

void swapStereo(float* interleaved, int frames)
{
    auto L = gam::slice(interleaved, frames, 2, 0);
    auto R = gam::slice(interleaved, frames, 2, 1);
    L.swap(R);
}
}