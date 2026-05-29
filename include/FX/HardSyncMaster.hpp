class HardSyncMaster : public Generator
{
public:
    HardSyncMaster(float masterFreq = 110.f, float slaveFreq = 440.f)
    {
        setMasterFreq(masterFreq);
        setSlaveFreq(slaveFreq);

        slave.setWaveform(Oscillator::SAW);

        setFM(0.f);
        setPM(0.f);
        setAM(1.f);
        setRatio(1.f);
        setIndex(1.f);
    }

    void setMasterFreq(float f)
    {
        masterFreq = std::max(0.f, f);
        masterPhase = -1.0;
    }

    void setSlaveFreq(float f)
    {
        baseFreq = f;
        slave.setFreq(f);
    }

    void setSlaveWave(Oscillator::Waveform w)
    {
        slave.setWaveform(w);
    }

    void setThroughZeroFM(bool e)
    {
        slave.setThroughZeroFM(e);
    }

    void reset()
    {
        masterPhase = -1.0;
        slave.setPhase(-1.0);
    }

    float process() override
    {
        // Advance master
        const double inc = (masterFreq / gam::sampleRate()) * 2.0;
        masterPhase += inc;

        if (masterPhase >= 1.0)
        {
            masterPhase -= 2.0;
            slave.hardSync();    // 🔥 band-limited hard sync
        }

        // Route ModGenerator modulation → slave
        slave.setFM(getFM());
        slave.setPM(getPM());
        slave.setAM(getAM());

        return slave.process();
    }

private:
    Oscillator slave;
    double masterFreq = 110.0;
    double masterPhase = -1.0;
};

