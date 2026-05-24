#pragma once
/*
  faust test.dsp \
  -lang cpp \
  -cn faust_absEnvRect \
  -namespace gdsp \
  -o absEnvRect.hpp
*/
#include "FaustMeta.hpp"

class faust_absEnvRect : public dsp {

 private:

        int IOTA0;
        float fRec0[2097152];
        FAUSTFLOAT fVslider0;
        int fSampleRate;
        float fConst0;

 public:
        faust_absEnvRect() {
        }

        void metadata(Meta* m) { 
                m->declare("analyzers.lib/abs_envelope_rect:author", "Dario Sanfilippo and Julius O. Smith III");
                m->declare("analyzers.lib/abs_envelope_rect:copyright", "Copyright (C) 2020 Dario Sanfilippo        <sanfilippo.dario@gmail.com> and         2003-2020 by Julius O. Smith III <jos@ccrma.stanford.edu>");
                m->declare("analyzers.lib/abs_envelope_rect:license", "MIT-style STK-4.3 license");
                m->declare("analyzers.lib/name", "Faust Analyzer Library");
                m->declare("analyzers.lib/version", "1.3.0");
                m->declare("basics.lib/name", "Faust Basic Element Library");
                m->declare("basics.lib/version", "1.22.0");
                m->declare("compile_options", "-lang cpp -ct 1 -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 0");
                m->declare("filename", "test.dp");
                m->declare("filters.lib/avg_rect:author", "Dario Sanfilippo and Julius O. Smith III");
                m->declare("filters.lib/avg_rect:copyright", "Copyright (C) 2020 Dario Sanfilippo       <sanfilippo.dario@gmail.com> and        2003-2020 by Julius O. Smith III <jos@ccrma.stanford.edu>");
                m->declare("filters.lib/avg_rect:license", "MIT-style STK-4.3 license");
                m->declare("filters.lib/integrator:author", "Julius O. Smith III");
                m->declare("filters.lib/integrator:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
                m->declare("filters.lib/integrator:license", "MIT-style STK-4.3 license");
                m->declare("filters.lib/lowpass0_highpass1", "MIT-style STK-4.3 license");
                m->declare("filters.lib/name", "Faust Filters Library");
                m->declare("filters.lib/version", "1.7.1");
                m->declare("maths.lib/author", "GRAME");
                m->declare("maths.lib/copyright", "GRAME");
                m->declare("maths.lib/license", "LGPL with exception");
                m->declare("maths.lib/name", "Faust Math Library");
                m->declare("maths.lib/version", "2.9.0");
                m->declare("name", "test.dp");
                m->declare("platform.lib/name", "Generic Platform Library");
                m->declare("platform.lib/version", "1.3.0");
        }

        virtual int getNumInputs() {
                return 1;
        }
        virtual int getNumOutputs() {
                return 1;
        }

        static void classInit(int sample_rate) {
        }

        virtual void instanceConstants(int sample_rate) {
                fSampleRate = sample_rate;
                fConst0 = std::min<float>(1.92e+05f, std::max<float>(1.0f, static_cast<float>(fSampleRate)));
        }

        virtual void instanceResetUserInterface() {
                fVslider0 = static_cast<FAUSTFLOAT>(1.0f);
        }

        virtual void instanceClear() {
                IOTA0 = 0;
                for (int l0 = 0; l0 < 2097152; l0 = l0 + 1) {
                        fRec0[l0] = 0.0f;
                }
        }

        virtual void init(int sample_rate) {
                classInit(sample_rate);
                instanceInit(sample_rate);
        }

        virtual void instanceInit(int sample_rate) {
                instanceConstants(sample_rate);
                instanceResetUserInterface();
                instanceClear();
        }

        virtual faust_absEnvRect* clone() {
                return new faust_absEnvRect();
        }

        virtual int getSampleRate() {
                return fSampleRate;
        }

        virtual void buildUserInterface(UI* ui_interface) {
                ui_interface->openVerticalBox("test.dp");
                ui_interface->addVerticalSlider("period", &fVslider0, FAUSTFLOAT(1.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+01f), FAUSTFLOAT(0.01f));
                ui_interface->closeBox();
        }

        virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
                FAUSTFLOAT* input0 = inputs[0];
                FAUSTFLOAT* output0 = outputs[0];
                float fSlow0 = std::rint(fConst0 * static_cast<float>(fVslider0));
                int iSlow1 = static_cast<int>(std::max<float>(0.0f, fSlow0));
                float fSlow2 = 1.0f / fSlow0;
                for (int i0 = 0; i0 < count; i0 = i0 + 1) {
                        fRec0[IOTA0 & 2097151] = std::fabs(static_cast<float>(input0[i0])) + fRec0[(IOTA0 - 1) & 2097151];
                        output0[i0] = static_cast<FAUSTFLOAT>(fSlow2 * (fRec0[IOTA0 & 2097151] - fRec0[(IOTA0 - iSlow1) & 2097151]));
                        IOTA0 = IOTA0 + 1;
                }
        }

};

class FAbsEnvRect : public FaustFunction {
public:
    explicit FAbsEnvRect(float sr)
    : FaustFunction(&mDsp)
    {
        mDsp.init((int)sr);
    }

private:
    faust_absEnvRect mDsp;
};
