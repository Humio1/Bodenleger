#pragma once
// Spectral analysis and resynthesis: the heart of Bodenleger.
// Source audio is measured as an averaged magnitude spectrum, split into tonal peaks and a smooth
// envelope, optionally masked to the notes of a key, and resynthesised with random phases into a
// seamless, endlessly looping stereo pad. Plain C++17, no framework dependencies.

#include <vector>
#include <array>
#include <random>
#include <cstdint>

namespace bl
{
constexpr int kAnalysisSize = 8192;
constexpr int kAnalysisHop  = 4096;
constexpr int kBins         = kAnalysisSize / 2 + 1;
constexpr int kSnapshots    = 3;

using Spec = std::vector<float>; // kBins magnitudes

void fft (double* re, double* im, int n, bool inverse);

struct Key
{
    int root  = 9; // 0 = C ... 11 = B
    int scale = 0; // 0 = minor, 1 = major, 2 = root + fifth
    bool operator== (const Key& o) const { return root == o.root && scale == o.scale; }
    bool operator!= (const Key& o) const { return ! (*this == o); }
};

std::vector<int> scaleIntervals (int scale);
std::vector<int> scaleNotes (Key k);

/** Average magnitude spectrum of a block of mono audio (up to 40 analysis frames). */
Spec spectrumOf (const float* x, int n);

/** Spectra of the first, middle and last third of a sound, normalised together. */
std::array<Spec, kSnapshots> thirdsSpectra (const std::vector<float>& mono);

void normaliseSet (std::array<Spec, kSnapshots>& set);
Spec smoothSpectrum (const Spec& m, float fraction);

struct SourceSpectra
{
    std::array<Spec, kSnapshots> spectra;
    float level = 1.0f;
    float tone  = 0.6f; // 1 = tonal peaks only, 0 = air (smooth envelope) only
};

struct Snapshot
{
    Spec tonal; // combined spectrum used for the raw and in-key layers
    Spec noise; // heavily smoothed spectrum used for the noise layer
};

std::array<Snapshot, kSnapshots> combine (const std::vector<SourceSpectra>& sources);
float snapshotEnergy (const std::array<Snapshot, kSnapshots>& snaps);

Key detectKey (const std::array<Snapshot, kSnapshots>& snaps, double sampleRate);

/** Gaussian weight around the nearest scale note, one value per bin of an FFT of size fftSize. */
std::vector<float> buildMask (Key key, int fftSize, double sampleRate, float sigmaSemitones);
std::vector<float> buildNoiseTilt (int fftSize, double sampleRate);

struct StereoLoop
{
    std::vector<float> left, right;
};

/** Resynthesise a spectrum into a seamless loop of length synthSize (power of two). */
StereoLoop synthesise (const Spec& spectrum, const float* fineWeights, int synthSize,
                       double sampleRate, std::mt19937& rng);

int synthSizeFor (double sampleRate);
} // namespace bl
