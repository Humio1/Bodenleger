#pragma once
// Small real-time building blocks: RBJ biquads, one-pole smoothers and a feedback-delay-network reverb.

#include <cmath>
#include <vector>
#include <array>
#include <algorithm>

namespace bl
{
constexpr double kPi = 3.14159265358979323846;

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    inline float process (float x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }

    void reset() noexcept { z1 = z2 = 0; }

    void set (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }

    static double clampF (double sr, double f) { return std::clamp (f, 10.0, sr * 0.45); }

    void lowpass (double sr, double f, double q) noexcept
    {
        const double w = 2 * kPi * clampF (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }

    void highpass (double sr, double f, double q) noexcept
    {
        const double w = 2 * kPi * clampF (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }

    void bandpass (double sr, double f, double q) noexcept // 0 dB peak gain
    {
        const double w = 2 * kPi * clampF (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }

    void lowShelf (double sr, double f, double db) noexcept
    {
        const double A = std::pow (10.0, db / 40.0), w = 2 * kPi * clampF (sr, f) / sr;
        const double c = std::cos (w), al = std::sin (w) / 2 * std::sqrt (2.0), sq = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }

    void highShelf (double sr, double f, double db) noexcept
    {
        const double A = std::pow (10.0, db / 40.0), w = 2 * kPi * clampF (sr, f) / sr;
        const double c = std::cos (w), al = std::sin (w) / 2 * std::sqrt (2.0), sq = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
};

struct Smoothed
{
    float value = 0, target = 0, coeff = 0.001f;
    void setTime (double sr, double seconds) { coeff = (float) (1.0 - std::exp (-1.0 / (seconds * sr))); }
    void snap (float v) { value = target = v; }
    inline float next() noexcept { value += (target - value) * coeff; return value; }
};

/** 8-line feedback delay network with Householder mixing and in-loop damping. */
class FDNReverb
{
public:
    static constexpr int N = 8;

    void prepare (double sr, double t60)
    {
        static const int base[N] = { 1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116 };
        const double scale = sr / 44100.0 * 2.2;
        for (int i = 0; i < N; ++i)
        {
            len[i] = std::max (64, (int) (base[i] * scale));
            buf[i].assign ((size_t) len[i], 0.0f);
            idx[i] = 0;
            lp[i] = 0.0f;
            gain[i] = (float) std::pow (10.0, -3.0 * len[i] / (t60 * sr));
        }
    }

    void reset()
    {
        for (int i = 0; i < N; ++i) { std::fill (buf[i].begin(), buf[i].end(), 0.0f); lp[i] = 0.0f; }
    }

    inline void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        float x[N];
        float sum = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const float v = buf[i][(size_t) idx[i]];
            lp[i] += (v - lp[i]) * (1.0f - damping);
            x[i] = lp[i];
            sum += x[i];
        }
        sum *= 2.0f / N;

        outL = (x[0] + x[2] + x[4] + x[6]) * 0.3f;
        outR = (x[1] + x[3] + x[5] + x[7]) * 0.3f;

        const float in = (inL + inR) * 0.35f;
        for (int i = 0; i < N; ++i)
        {
            const float y = (x[i] - sum) * gain[i] + ((i & 1) ? in : -in);
            buf[i][(size_t) idx[i]] = y;
            if (++idx[i] >= len[i]) idx[i] = 0;
        }
    }

    float damping = 0.32f;

private:
    std::array<std::vector<float>, N> buf;
    int len[N] {}, idx[N] {};
    float gain[N] {}, lp[N] {};
};
} // namespace bl
