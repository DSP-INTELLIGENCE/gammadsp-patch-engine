template <class Sample>
struct Transfer {
    Sample T = (Sample)-24;  // threshold dB
    Sample R = (Sample)4;    // ratio >= 1

    // returns gain in dB (negative for attenuation)
    Sample gainDbFromLevelDb(Sample L) const {
        if (L <= T) return (Sample)0;
        Sample GR = (L - T) * ((Sample)1 - (Sample)1 / R);
        return -GR;
    }
};
