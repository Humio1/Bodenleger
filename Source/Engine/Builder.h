#pragma once
// Turns analysed sources into a ready-to-play LayerSet (runs on a background thread).

#include "Engine.h"

namespace bl
{
struct BuildResult
{
    LayerSet* set = nullptr; // ownership passes to the caller
    std::array<Snapshot, kSnapshots> snapshots;
    Key key;
    std::vector<float> displayMask; // per analysis bin, for drawing
};

class LayerBuilder
{
public:
    /** Returns a result with set == nullptr when the sources are silent. */
    BuildResult build (const std::vector<SourceSpectra>& sources,
                       std::vector<std::pair<std::shared_ptr<const SampleData>, float>> grainSources,
                       bool autoKey, Key manualKey, int keyMode, double sampleRate);

private:
    std::mt19937 rng { std::random_device{}() };
    Key maskKey { -1, -1 };
    int maskMode = -1;
    int maskSize = 0;
    double maskRate = 0;
    std::vector<float> fineMask, noiseTilt, coarseMask;
};

/** Prepare a sample for use: mono mix, spectra and drawing peaks. Audio must already be at the host rate. */
std::shared_ptr<SampleData> makeSampleData (std::string name, std::vector<float> left, std::vector<float> right);
} // namespace bl
