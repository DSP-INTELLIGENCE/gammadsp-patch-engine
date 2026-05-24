
#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class VCA {
public:
    void setMakeupDb(Sample db)
    {
        makeup = std::pow((Sample)10, db / (Sample)20);
    }

    void setDryWet(Sample w)
    {
        wet = std::clamp(w, (Sample)0, (Sample)1);
    }

    void reset() {}

    inline Sample process(Sample input, Sample controlGain)
    {
        const Sample y = input * controlGain * makeup;
        return ((Sample)1 - wet) * input + wet * y;
    }

private:
    Sample makeup { (Sample)1 };
    Sample wet    { (Sample)1 };
};

/*
Excellent — this is the final **audio-side** piece of the machine.

Everything you’ve built so far is the **control system**.
The **VCA** is where that control voltage becomes sound.

Let’s design a clean, professional digital VCA.

---

# 🧱 What a **VCA** Is in DSP

In analog:

> a circuit whose gain is controlled by a control voltage.

In digital:

[
y[n] = x[n] \cdot g[n]
]

Where:

* (x[n]) = audio sample
* (g[n]) = control signal from your `GainController` (linear)

That’s it — but the details matter.

---

# 🧮 Core VCA Model

```
Control Signal (dB) → convert to linear → multiply audio
```

Because you already smooth the gain in dB, the VCA itself is **stateless and clean**.

---

# 🧬 Where It Sits in Your Engine

Your complete compressor now has:

```
Audio In
   ├──► Detector → Control Signal Generator → GainController → controlGain
   │
   └──► Lookahead → VCA → Output
```

The **VCA** is the last transformation before the output.

---

# 🎚 Why This Design Is Correct

| Design Choice                    | Why                          |
| -------------------------------- | ---------------------------- |
| Gain smoothing in control domain | eliminates zipper noise      |
| Stateless VCA                    | perfectly linear & stable    |
| Dry/Wet inside VCA               | correct parallel compression |
| Makeup in VCA                    | keeps metering consistent    |

This exactly matches how modern digital VCAs are implemented.

---

# 🧭 How It Feels Sonically

The sound of your compressor is now defined by:

* your **detector + character engine**
* your **transfer function**
* your **gain controller**
* **not** by the VCA

Which is exactly how real compressors work.

---

If you'd like next, we can add **VCA nonlinearity** (THD, saturation, control bleed) to emulate specific analog VCAs like THAT2181, DBX, and SSL.
*/