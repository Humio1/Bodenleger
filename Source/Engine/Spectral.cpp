#include "Spectral.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace bl
{
namespace
{
constexpr double kTwoPi = 6.283185307179586476925;

const std::vector<double>& hann()
{
    static const std::vector<double> w = []
    {
        std::vector<double> v (kAnalysisSize);
        for (int i = 0; i < kAnalysisSize; ++i)
            v[(size_t) i] = 0.5 - 0.5 * std::cos (kTwoPi * i / kAnalysisSize);
        return v;
    }();
    return w;
}

double midiOf (double hz) { return 69.0 + 12.0 * std::log2 (hz / 440.0); }

double nearestScaleDistance (double midi, const bool* inScale)
{
    const int r = (int) std::lround (midi);
    double d = 99.0;
    for (int o = -6; o <= 6; ++o)
    {
        const int c = r + o;
        if (inScale[((c % 12) + 12) % 12])
            d = std::min (d, std::abs (midi - c));
    }
    return d;
}
} // namespace

void fft (double* re, double* im, int n, bool inverse)
{
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
        {
            std::swap (re[i], re[j]);
            std::swap (im[i], im[j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        const double ang = kTwoPi / len * (inverse ? 1.0 : -1.0);
        const double wr = std::cos (ang), wi = std::sin (ang);
        const int half = len >> 1;
        for (int i = 0; i < n; i += len)
        {
            double cr = 1.0, ci = 0.0;
            for (int k = 0; k < half; ++k)
            {
                const int a = i + k, b = a + half;
                const double tr = re[b] * cr - im[b] * ci;
                const double ti = re[b] * ci + im[b] * cr;
                re[b] = re[a] - tr;
                im[b] = im[a] - ti;
                re[a] += tr;
                im[a] += ti;
                const double ncr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = ncr;
            }
        }
    }
}

std::vector<int> scaleIntervals (int scale)
{
    switch (scale)
    {
        case 1:  return { 0, 2, 4, 5, 7, 9, 11 };
        case 2:  return { 0, 7 };
        default: return { 0, 2, 3, 5, 7, 8, 10 };
    }
}

std::vector<int> scaleNotes (Key k)
{
    auto iv = scaleIntervals (k.scale);
    for (auto& i : iv)
        i = (k.root + i) % 12;
    return iv;
}

std::vector<int> maskNotes (Key k, int keyMode)
{
    if (keyMode != kChord) return scaleNotes (k);
    const auto iv = scaleIntervals (k.scale);
    std::vector<int> out;
    if (iv.size() == 7)
        for (int idx : { 0, 2, 4, 6 }) out.push_back ((k.root + iv[(size_t) idx]) % 12);
    else
        for (int i : iv) out.push_back ((k.root + i) % 12);
    return out;
}

Spec spectrumOf (const float* x, int n)
{
    const auto& win = hann();
    std::vector<double> pow ((size_t) kBins, 0.0), re ((size_t) kAnalysisSize), im ((size_t) kAnalysisSize);
    const int span = std::max (0, n - kAnalysisSize);
    const int frames = std::max (1, std::min (40, span / kAnalysisHop + 1));
    const double step = frames > 1 ? (double) span / (frames - 1) : 0.0;

    for (int f = 0; f < frames; ++f)
    {
        const int p = (int) (f * step);
        for (int i = 0; i < kAnalysisSize; ++i)
        {
            const float v = p + i < n ? x[p + i] : 0.0f;
            re[(size_t) i] = v * win[(size_t) i];
            im[(size_t) i] = 0.0;
        }
        fft (re.data(), im.data(), kAnalysisSize, false);
        for (int k = 0; k < kBins; ++k)
            pow[(size_t) k] += re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k];
    }

    Spec m ((size_t) kBins);
    for (int k = 0; k < kBins; ++k)
        m[(size_t) k] = (float) std::sqrt (pow[(size_t) k] / frames);
    return m;
}

void normaliseSet (std::array<Spec, kSnapshots>& set)
{
    double e = 0.0;
    for (auto& m : set)
        for (float v : m)
            e += (double) v * v;
    const double s = 1.0 / std::sqrt (e / (kBins * kSnapshots) + 1e-20);
    for (auto& m : set)
        for (auto& v : m)
            v = (float) (v * s);
}

std::array<Spec, kSnapshots> thirdsSpectra (const std::vector<float>& mono)
{
    std::array<Spec, kSnapshots> out;
    const int L = (int) mono.size();
    for (int s = 0; s < kSnapshots; ++s)
    {
        int a = s * L / kSnapshots, b = (s + 1) * L / kSnapshots;
        if (b - a < kAnalysisSize) { a = 0; b = L; }
        out[(size_t) s] = spectrumOf (mono.data() + a, b - a);
    }
    normaliseSet (out);
    return out;
}

Spec smoothSpectrum (const Spec& m, float fraction)
{
    std::vector<double> ps ((size_t) kBins + 1, 0.0);
    for (int k = 0; k < kBins; ++k)
        ps[(size_t) k + 1] = ps[(size_t) k] + m[(size_t) k];
    Spec out ((size_t) kBins);
    for (int k = 0; k < kBins; ++k)
    {
        const int h = std::max (2, (int) std::lround (k * fraction));
        const int a = std::max (0, k - h), b = std::min (kBins, k + h + 1);
        out[(size_t) k] = (float) ((ps[(size_t) b] - ps[(size_t) a]) / (b - a));
    }
    return out;
}

std::array<Snapshot, kSnapshots> combine (const std::vector<SourceSpectra>& sources)
{
    std::array<Snapshot, kSnapshots> snaps;
    for (int s = 0; s < kSnapshots; ++s)
    {
        Spec C ((size_t) kBins, 0.0f), Nz ((size_t) kBins, 0.0f);
        for (const auto& src : sources)
        {
            const auto& m = src.spectra[(size_t) s];
            const auto E  = smoothSpectrum (m, 0.03f);
            const auto E2 = smoothSpectrum (m, 0.12f);
            const float t = src.tone, l = src.level;
            for (int k = 0; k < kBins; ++k)
            {
                const float P = std::max (0.0f, m[(size_t) k] - E[(size_t) k] * 1.15f);
                C[(size_t) k]  += l * (t * (P * 1.8f + E[(size_t) k] * 0.2f) + (1.0f - t) * E[(size_t) k]);
                Nz[(size_t) k] += l * E2[(size_t) k];
            }
        }
        snaps[(size_t) s].tonal = std::move (C);
        snaps[(size_t) s].noise = std::move (Nz);
    }
    return snaps;
}

float snapshotEnergy (const std::array<Snapshot, kSnapshots>& snaps)
{
    double e = 0.0;
    for (const auto& s : snaps)
        for (float v : s.tonal)
            e += (double) v * v;
    return (float) e;
}

Key detectKey (const std::array<Snapshot, kSnapshots>& snaps, double sampleRate)
{
    static const double maj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double min[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

    double chroma[12] = {};
    const double binHz = sampleRate / kAnalysisSize;
    for (int k = 1; k < kBins; ++k)
    {
        const double f = k * binHz;
        if (f < 60.0 || f > 4200.0) continue;
        const int pc = (((int) std::lround (midiOf (f))) % 12 + 12) % 12;
        for (const auto& sn : snaps)
            chroma[pc] += sn.tonal[(size_t) k];
    }

    auto corr = [] (const double* a, const double* b)
    {
        double ma = 0, mb = 0;
        for (int i = 0; i < 12; ++i) { ma += a[i]; mb += b[i]; }
        ma /= 12; mb /= 12;
        double n = 0, da = 0, db = 0;
        for (int i = 0; i < 12; ++i)
        {
            const double x = a[i] - ma, y = b[i] - mb;
            n += x * y; da += x * x; db += y * y;
        }
        return n / std::sqrt (da * db + 1e-20);
    };

    Key best;
    double bestR = -2.0;
    for (int root = 0; root < 12; ++root)
    {
        for (int sc = 0; sc < 2; ++sc)
        {
            const double* prof = sc == 0 ? min : maj;
            double rot[12];
            for (int i = 0; i < 12; ++i)
                rot[i] = prof[(i - root + 12) % 12];
            const double r = corr (chroma, rot);
            if (r > bestR) { bestR = r; best = { root, sc == 0 ? 0 : 1 }; }
        }
    }
    return best;
}

std::vector<float> buildMask (const std::vector<int>& notes, int fftSize, double sampleRate, float sigma)
{
    bool inScale[12] = {};
    for (int n : notes)
        inScale[((n % 12) + 12) % 12] = true;

    const int half = fftSize / 2;
    const double df = sampleRate / fftSize, s2 = 2.0 * sigma * sigma;
    std::vector<float> m ((size_t) half + 1, 0.0f);
    for (int j = 1; j <= half; ++j)
    {
        const double f = j * df;
        if (f < 30.0) continue;
        const double d = nearestScaleDistance (midiOf (f), inScale);
        m[(size_t) j] = (float) std::exp (-(d * d) / s2);
    }
    return m;
}

std::vector<float> buildNoiseTilt (int fftSize, double sampleRate)
{
    const int half = fftSize / 2;
    std::vector<float> t ((size_t) half + 1, 0.0f);
    for (int j = 1; j <= half; ++j)
    {
        const double f = j * sampleRate / fftSize;
        t[(size_t) j] = (float) std::min (4.0, std::pow (std::max (f, 60.0) / 1000.0, 0.4));
    }
    return t;
}

int synthSizeFor (double sampleRate)
{
    return sampleRate > 64000.0 ? (1 << 19) : (1 << 18);
}

StereoLoop synthesise (const Spec& S, const float* fine, int N, double sampleRate, std::mt19937& rng, float octaveUp)
{
    std::vector<double> re ((size_t) N, 0.0), im ((size_t) N, 0.0);
    std::uniform_real_distribution<double> phase (0.0, kTwoPi);
    const int half = N / 2;
    const double ratio = (double) kAnalysisSize / N, df = sampleRate / N;
    const double top = std::min (16500.0, sampleRate * 0.45);

    for (int j = 1; j < half; ++j)
    {
        const double f = j * df;
        if (f < 32.0 || f > top) continue;
        const double x = j * ratio;
        const int i0 = (int) x;
        const double fr = x - i0;
        double mag = S[(size_t) i0] * (1.0 - fr) + S[(size_t) std::min (kBins - 1, i0 + 1)] * fr;
        if (octaveUp > 0.0f)
        {
            const double xh = x * 0.5;
            const int h0 = (int) xh;
            if (h0 >= 1)
            {
                const double hf = xh - h0;
                mag += octaveUp * (S[(size_t) h0] * (1.0 - hf) + S[(size_t) std::min (kBins - 1, h0 + 1)] * hf);
            }
        }
        if (fine != nullptr) mag *= fine[j];
        if (f < 90.0) mag *= (f - 32.0) / 58.0;
        if (f > 12000.0) mag *= std::max (0.0, (top - f) / (top - 12000.0));
        if (! (mag > 0.0)) continue;
        const double ph = phase (rng);
        re[(size_t) j] = mag * std::cos (ph);
        im[(size_t) j] = mag * std::sin (ph);
        re[(size_t) (N - j)] = re[(size_t) j];
        im[(size_t) (N - j)] = -im[(size_t) j];
    }

    fft (re.data(), im.data(), N, true);

    double e = 0.0;
    for (double v : re) e += v * v;
    const double g = e > 0.0 ? 0.12 / std::sqrt (e / N) : 0.0;

    StereoLoop out;
    out.left.resize ((size_t) N);
    out.right.resize ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        out.left[(size_t) i]  = (float) (re[(size_t) i] * g);
        out.right[(size_t) i] = (float) (re[(size_t) ((i + half) % N)] * g);
    }
    return out;
}
} // namespace bl
