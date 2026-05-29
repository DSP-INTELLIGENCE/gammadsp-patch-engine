#include <stdio.h>
#define GAMMA_H_INC_ALL 1
#include "Gamma/Gamma.h"

namespace gam {
class AudioApp : public AudioCallback{
public:

	AudioApp(){
		mAudioIO.append(*this);
		initAudio(44100);
	}

	void initAudio(
		double framesPerSec, unsigned framesPerBuffer=128,
		unsigned outChans=2, unsigned inChans=0
	){
		mAudioIO.framesPerSecond(framesPerSec);
		mAudioIO.framesPerBuffer(framesPerBuffer);
		mAudioIO.channelsOut(outChans);
		mAudioIO.channelsIn(inChans);
		sampleRate(framesPerSec);
	}

	AudioIO& audioIO(){ return mAudioIO; }

	void start(bool block=true){
		mAudioIO.start();
		if(block){		
			printf("Press 'enter' to quit...\n");
			fflush(stdout);
			getchar();
		}
	}

private:
	AudioIO mAudioIO;
};
}