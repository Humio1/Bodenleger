#include "Builder.h"
#include <cmath>

namespace bl
{
BuildResult LayerBuilder::build (const std::vector<SourceSpectra>& sources,
                                 std::vector<std::pair<std::shared_ptr<const SampleData>, float>> grainSources,
                                 bool autoKey, Key manualKey, double sr)
{
    BuildResult r;
    r.key = manualKey;
    if (sources.empty()) return r;

    r.snapshots = combine (sources);
    if (snapshotEnergy (r.snapshots) < 1e-12f) return r;

    if (autoKey) r.key = detectKey (r.snapshots, sr);

    const int N = synthSizeFor (sr);
    if (r.key != maskKey || N != maskSize || sr != maskRate)
    {
        fineMask   = buildMask (r.key, N, sr, 0.09f);
        coarseMask = buildMask (r.key, kAnalysisSize, sr, 0.3f);
        if (N != maskSize || sr != maskRate) noiseTilt = buildNoiseTilt (N, sr);
        maskKey = r.key; maskSize = N; maskRate = sr;
    }
    r.displayMask = coarseMask;

    auto* set = new LayerSet();
    set->length = N;
    for (int k = 0; k < kSnapshots; ++k)
    {
        const auto& sn = r.snapshots[(size_t) k];
        set->raw[(size_t) k]   = synthesise (sn.tonal, nullptr, N, sr, rng);
        set->inKey[(size_t) k] = synthesise (sn.tonal, fineMask.data(), N, sr, rng);
        set->noise[(size_t) k] = synthesise (sn.noise, noiseTilt.data(), N, sr, rng);
    }
    set->grainSources = std::move (grainSources);
    r.set = set;
    return r;
}

std::shared_ptr<SampleData> makeSampleData (std::string name, std::vector<float> left, std::vector<float> right)
{
    auto d = std::make_shared<SampleData>();
    d->name = std::move (name);
    if (right.size() != left.size()) right = left;
    d->mono.resize (left.size());
    for (size_t i = 0; i < left.size(); ++i)
        d->mono[i] = 0.5f * (left[i] + right[i]);
    d->left = std::move (left);
    d->right = std::move (right);
    if (d->mono.size() < 64) d->mono.resize (64, 0.0f);
    d->spectra = thirdsSpectra (d->mono);

    const int cols = 1024;
    d->peaks.assign ((size_t) cols * 2, 0.0f);
    const double step = (double) d->mono.size() / cols;
    for (int c = 0; c < cols; ++c)
    {
        float mn = 0, mx = 0;
        const size_t a = (size_t) (c * step), b = std::min (d->mono.size(), (size_t) ((c + 1) * step) + 1);
        for (size_t i = a; i < b; ++i) { mn = std::min (mn, d->mono[i]); mx = std::max (mx, d->mono[i]); }
        d->peaks[(size_t) c * 2] = mn;
        d->peaks[(size_t) c * 2 + 1] = mx;
    }
    return d;
}
} // namespace bl
