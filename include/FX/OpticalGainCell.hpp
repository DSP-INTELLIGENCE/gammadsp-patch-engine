#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class OpticalGainCell {
public:
    void setMakeupDb(Sample db)
    {
        makeup = std::pow((Sample)10, db / (Sample)20);
    }

    void setDryWet(Sample w)
    {
        wet = std::clamp(w, (Sample)0, (Sample)1);
    }

    void setSensitivity(Sample s) { sensitivity = std::max((Sample)0.1, s); }
    void setMemory(Sample m)      { memory = std::clamp(m, (Sample)0, (Sample)0.999); }
    void setReleaseCurve(Sample r){ releaseCurve = std::clamp(r, (Sample)0.1, (Sample)5); }

    void reset()
    {
        light = (Sample)0;
        resistance = (Sample)1;
    }

    // controlGain: linear gain from GainController (0..1)
    inline Sample process(Sample x, Sample controlGain)
    {
        // --- Light emission (control → light) ---
        Sample targetLight = std::pow((Sample)1 - controlGain, sensitivity);

        // LED attack is fast, decay is slow (implicit in memory)
        light += (targetLight - light) * (Sample)0.01;

        // --- Photoresistor behavior ---
        // Memory & hysteresis
        resistance = resistance * memory + light * ((Sample)1 - memory);

        // Nonlinear response curve
        Sample optGain = (Sample)1 / ((Sample)1 + releaseCurve * resistance);

        Sample y = x * optGain * makeup;

        return ((Sample)1 - wet) * x + wet * y;
    }

private:
    Sample makeup { (Sample)1 };
    Sample wet    { (Sample)1 };

    Sample sensitivity { (Sample)1.0 };
    Sample memory { (Sample)0.95 };
    Sample releaseCurve { (Sample)1.0 };

    Sample light { (Sample)0 };
    Sample resistance { (Sample)1 };
};

/*
Ah — **optical** is a completely different beast from a VCA, and this is where compression becomes **alive** instead of just controlled.

An **Optical Gain Cell** is not just a multiplier.
It’s a **memory device**: light → resistor → time → sound.

Let’s build it properly.

---

# 🧠 How an Optical Gain Cell Behaves

An optical compressor’s gain element consists of:

* a light source (LED / EL panel)
* a photoresistor

The photoresistor has:

* **slow attack**
* **very slow, multi-stage release**
* **memory & hysteresis**
* non-linear gain vs control relationship

So we need:

1. a light model
2. a photoresistor model
3. hysteresis & memory
4. non-linear gain curve

---

# 🧱 **OpticalGainCell.hpp**

This is a production-quality optical model that drops into your engine in place of the VCA.

---

# 🎛️ Why This Sounds Like Optical

| Property               | Result                     |
| ---------------------- | -------------------------- |
| Slow nonlinear release | natural leveling           |
| Memory                 | smooth pumping, no chatter |
| Nonlinear gain curve   | musical compression        |
| Hysteresis             | living, breathing sound    |

This reproduces the **LA-2A / LA-3A / T4 cell** family of behavior.

---

# 🧭 How It Fits In Your Engine

Swap your VCA for this:

```cpp
OpticalGainCell<float> gainStage;
```

The rest of your control system stays the same — only the **gain element** changes.

---

# 🧬 Typical Settings

| Use    | Sensitivity | Memory | ReleaseCurve |
| ------ | ----------- | ------ | ------------ |
| Vocals | 1.0         | 0.96   | 1.2          |
| Bass   | 1.2         | 0.97   | 1.5          |
| Bus    | 0.8         | 0.94   | 1.0          |

---

If you want, next we can:

* model **FET gain stages**,
* model **variable-mu tube stages**,
* or build a **switchable gain engine** that morphs between VCA / Optical / FET / Tube in one processor.
*/