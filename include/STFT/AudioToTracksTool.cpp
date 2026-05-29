#include <cstdio>
#include <vector>
#include <utility>
#include "LorisTracks.hpp"

class AudioToTracksTool {
public:
    explicit AudioToTracksTool(unsigned blockSize = 4096)
    : m_blockSize(blockSize) {}

    bool processFile(const char* path, TrackModel& outModel)
    {
        // -----------------------------------------
        // Open audio file
        // -----------------------------------------
        SF_INFO info{};
        SNDFILE* file = sf_open(path, SFM_READ, &info);
        if (!file || info.channels != 1)
            return false;

        outModel.sampleRate = info.samplerate;

        // -----------------------------------------
        // Configure Loris ONCE
        // -----------------------------------------
        analyzer_configure(
            20.0,   // frequency resolution (Hz)
            0.0     // auto window
        );

        PartialList* partials = createPartialList();

        // -----------------------------------------
        // Block-based analysis
        // -----------------------------------------
        std::vector<float>  fblock(m_blockSize);
        std::vector<double> dblock(m_blockSize);

        while (true) {
            sf_count_t n =
                sf_readf_float(file, fblock.data(), m_blockSize);
            if (n <= 0) break;

            for (sf_count_t i = 0; i < n; ++i)
                dblock[i] = static_cast<double>(fblock[i]);

            analyze(dblock.data(),
                    static_cast<unsigned>(n),
                    outModel.sampleRate,
                    partials);

            if (n < static_cast<sf_count_t>(m_blockSize))
                break;
        }

        sf_close(file);

        // -----------------------------------------
        // Convert Loris PartialList → TrackModel
        // -----------------------------------------
        extractTracks(partials, outModel.tracks);
        destroyPartialList(partials);

        return true;
    }

private:
    unsigned m_blockSize;

    static void extractTracks(const PartialList* plist,
                              std::vector<PartialTrack>& out)
    {
        forEachPartial(const_cast<PartialList*>(plist),
            [](Partial* p, void* ctx) -> int {

                auto& tracks =
                    *static_cast<std::vector<PartialTrack>*>(ctx);

                PartialTrack track;
                track.label     = partial_label(p);
                track.startTime = partial_startTime(p);
                track.endTime   = partial_endTime(p);
                if (track.breakpoints.size() < 2)
                    return 0;

                forEachBreakpoint(p,
                    [](Breakpoint* bp, double t, void* ctx) -> int {
                        auto& tr =
                            *static_cast<PartialTrack*>(ctx);

                        tr.breakpoints.push_back({
                            t,
                            breakpoint_getFrequency(bp),
                            breakpoint_getAmplitude(bp),
                            breakpoint_getBandwidth(bp),
                            breakpoint_getPhase(bp)
                        });
                        return 0;
                    },
                    &track
                );
                
                tracks.push_back(std::move(track));
                return 0;
            },
            &out
        );
    }
};

static void writeDouble(FILE* f, double x)
{
    std::fprintf(f, "%.10g", x);
}

bool writeTrackModelJson(const char* path, const TrackModel& model)
{
    FILE* f = std::fopen(path, "w");
    if (!f) return false;
    std::fprintf(f, "{\n  \"format\": \"loris-tracks\",\n  \"version\": 1,\n");

    std::fprintf(f, "\n   \"sampleRate\": ");
    writeDouble(f, model.sampleRate);
    std::fprintf(f, ",\n  \"tracks\": [\n");

    for (size_t i = 0; i < model.tracks.size(); ++i)
    {
        const auto& t = model.tracks[i];
        if (i) std::fprintf(f, ",\n");

        std::fprintf(f,
            "    {\n"
            "      \"label\": %d,\n"
            "      \"startTime\": ", t.label);
        writeDouble(f, t.startTime);
        std::fprintf(f, ",\n      \"endTime\": ");
        writeDouble(f, t.endTime);
        std::fprintf(f, ",\n      \"breakpoints\": [\n");

        for (size_t k = 0; k < t.breakpoints.size(); ++k)
        {
            const auto& bp = t.breakpoints[k];
            if (k) std::fprintf(f, ",\n");

            std::fprintf(f, "        { \"time\": ");
            writeDouble(f, bp.time);
            std::fprintf(f, ", \"frequency\": ");
            writeDouble(f, bp.frequency);
            std::fprintf(f, ", \"amplitude\": ");
            writeDouble(f, bp.amplitude);
            std::fprintf(f, ", \"bandwidth\": ");
            writeDouble(f, bp.bandwidth);
            std::fprintf(f, ", \"phase\": ");
            writeDouble(f, bp.phase);
            std::fprintf(f, " }");
        }

        std::fprintf(f, "\n      ]\n    }");
    }

    std::fprintf(f, "\n  ]\n}\n");
    std::fclose(f);
    return true;
}

bool loadTrackModelJson(const char* path, TrackModel& model)
{
    Json doc = parseJsonFile(path); // your JSON lib

    model.sampleRate = doc["sampleRate"].asDouble();
    model.tracks.clear();

    for (auto& jt : doc["tracks"])
    {
        PartialTrack t;
        t.label     = jt["label"].asInt();
        t.startTime = jt["startTime"].asDouble();
        t.endTime   = jt["endTime"].asDouble();

        for (auto& jb : jt["breakpoints"])
        {
            TrackBreakpoint bp;
            bp.time      = jb["time"].asDouble();
            bp.frequency = jb["frequency"].asDouble();
            bp.amplitude = jb["amplitude"].asDouble();
            bp.bandwidth = jb["bandwidth"].asDouble();
            bp.phase     = jb["phase"].asDouble();

            t.breakpoints.push_back(bp);
        }

        model.tracks.push_back(std::move(t));
    }

    return true;
}

int main(int argc, const char* argv[])
{
    if (argc < 3) {
        fprintf(stderr, "usage: tool input.wav output.json\n");
        return 1;
    }

    TrackModel model;
    AudioToTracksTool tool;

    if (!tool.processFile(argv[1], model)) {
        fprintf(stderr, "analysis failed\n");
        return 1;
    }

    if (!writeTrackModelJson(argv[2], model)) {
        fprintf(stderr, "write failed\n");
        return 1;
    }

    return 0;
}
