// Offline test for the Bodenleger engine: renders both sources and both modes with generated
// demo material, checks the output is sane, and writes WAV files for listening.
// Build: c++ -std=c++17 -O2 -I../Source tests/EngineTest.cpp ../Source/Engine/*.cpp -o engine_test

#include "Engine/Builder.h"
#include <cstdio>
#include <cmath>
#include <fstream>
#include <functional>

using namespace bl;
static const double SR = 48000.0;

static double mtof (double m) { return 440.0 * std::pow (2.0, (m - 69) / 12.0); }

struct Buf { std::vector<float> L, R; };

static Buf makeBuf (double sec, const std::function<void (std::vector<float>&, std::vector<float>&, int)>& fn)
{
    Buf b; const int n = (int) (sec * SR); b.L.assign ((size_t) n, 0); b.R.assign ((size_t) n, 0);
    fn (b.L, b.R, n); return b;
}

static Buf chords()
{
    const int prog[4][4] = { { 57, 60, 64, 67 }, { 53, 57, 60, 64 }, { 48, 55, 60, 64 }, { 55, 59, 62, 67 } };
    return makeBuf (8, [&] (auto& L, auto& R, int n)
    {
        for (int c = 0; c < 4; ++c)
            for (int m : prog[c])
                for (int ch = 0; ch < 2; ++ch)
                    for (int h = 1; h <= 8; ++h)
                    {
                        const double f = mtof (m) * (ch ? 1.002 : 0.998) * h, a = 0.07 / h;
                        auto& d = ch ? R : L;
                        for (int i = (int) (c * 2 * SR); i < (int) ((c + 1) * 2 * SR) && i < n; ++i)
                        {
                            const double t = (i - c * 2 * SR) / SR;
                            const double env = std::min (1.0, t / 0.04) * std::min (1.0, (2 - t) / 0.15);
                            d[(size_t) i] += (float) (a * env * std::sin (2 * M_PI * f * i / SR));
                        }
                    }
    });
}

static Buf snare()
{
    return makeBuf (8, [] (auto& L, auto& R, int n)
    {
        uint32_t s = 1; double lp = 0;
        for (double hit = 0.5; hit < 8; hit += 1)
            for (int i = 0; i < (int) (SR * 0.4) && (int) (hit * SR) + i < n; ++i)
            {
                s = s * 1664525u + 1013904223u; const double w = (s >> 8) / 8388608.0 - 1.0;
                const double t = i / SR; lp += (w - lp) * 0.55;
                const float v = (float) (lp * std::exp (-t * 16) * 0.55 + std::sin (2 * M_PI * 185 * t) * std::exp (-t * 24) * 0.45);
                L[(size_t) (hit * SR) + (size_t) i] += v; R[(size_t) (hit * SR) + (size_t) i] += v;
            }
    });
}

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R)
{
    std::ofstream f (path, std::ios::binary);
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 4;
    auto w32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto w16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); w32 (36 + dataBytes); f.write ("WAVEfmt ", 8); w32 (16); w16 (1); w16 (2);
    w32 ((uint32_t) SR); w32 ((uint32_t) SR * 4); w16 (4); w16 (16); f.write ("data", 4); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t a = (int16_t) std::lround (std::clamp (L[i], -1.0f, 1.0f) * 32767);
        int16_t b = (int16_t) std::lround (std::clamp (R[i], -1.0f, 1.0f) * 32767);
        f.write ((const char*) &a, 2); f.write ((const char*) &b, 2);
    }
}

static const char* keyName (Key k)
{
    static char buf[32];
    static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    std::snprintf (buf, sizeof buf, "%s %s", names[k.root], k.scale == 0 ? "minor" : k.scale == 1 ? "major" : "5th");
    return buf;
}

static int failures = 0;

static void render (Engine& e, const Buf* input, double seconds, EngineParams p, std::vector<float>& outL, std::vector<float>& outR,
                    const Buf* sidechain = nullptr, float* maxDuck = nullptr)
{
    const int block = 512, total = (int) (seconds * SR);
    std::vector<float> il (block), ir (block), ol (block), orr (block), sl (block), sr (block);
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        for (int i = 0; i < n; ++i)
        {
            il[(size_t) i] = input ? input->L[(size_t) ((pos + i) % (int) input->L.size())] : 0.0f;
            ir[(size_t) i] = input ? input->R[(size_t) ((pos + i) % (int) input->R.size())] : 0.0f;
            sl[(size_t) i] = sidechain ? sidechain->L[(size_t) ((pos + i) % (int) sidechain->L.size())] : 0.0f;
            sr[(size_t) i] = sidechain ? sidechain->R[(size_t) ((pos + i) % (int) sidechain->R.size())] : 0.0f;
        }
        if (sidechain) { p.sidechainL = sl.data(); p.sidechainR = sr.data(); }
        p.lfoPhase = -1.0;
        e.process (il.data(), ir.data(), ol.data(), orr.data(), n, p);
        if (maxDuck) *maxDuck = std::max (*maxDuck, e.duckAmount.load());
        outL.insert (outL.end(), ol.begin(), ol.begin() + n);
        outR.insert (outR.end(), orr.begin(), orr.begin() + n);
        while (auto* r = e.takeRetired()) delete r;
    }
}

static void check (const char* label, const std::vector<float>& L, const std::vector<float>& R, size_t skip)
{
    double e = 0; float peak = 0; bool finite = true;
    for (size_t i = skip; i < L.size(); ++i)
    {
        if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) finite = false;
        e += L[i] * L[i] + R[i] * R[i];
        peak = std::max (peak, std::max (std::abs (L[i]), std::abs (R[i])));
    }
    const double rms = std::sqrt (e / (2.0 * (L.size() - skip)));
    const bool ok = finite && rms > 0.01 && peak <= 1.0f;
    std::printf ("%-28s rms %.3f  peak %.3f  %s\n", label, rms, peak, ok ? "OK" : "FAIL");
    if (! ok) ++failures;
}

static Buf kicks()
{
    return makeBuf (8, [] (auto& L, auto& R, int n)
    {
        for (double hit = 0; hit < 8; hit += 0.5)
        {
            double ph = 0;
            for (int i = 0; i < (int) (SR * 0.3) && (int) (hit * SR) + i < n; ++i)
            {
                const double t = i / SR; ph += 2 * M_PI * (50 + 120 * std::exp (-t * 30)) / SR;
                const float v = (float) (std::sin (ph) * std::exp (-t * 9) * 0.8);
                L[(size_t) (hit * SR) + (size_t) i] += v; R[(size_t) (hit * SR) + (size_t) i] += v;
            }
        }
    });
}

/** Ratio between the loudest and quietest 50 ms window (in dB) after skip. */
static double swingDb (const std::vector<float>& L, size_t skip)
{
    const size_t w = (size_t) (0.05 * SR);
    double lo = 1e9, hi = 0;
    for (size_t a = skip; a + w < L.size(); a += w)
    {
        double e = 0; for (size_t i = a; i < a + w; ++i) e += L[i] * L[i];
        e = std::sqrt (e / w); lo = std::min (lo, e); hi = std::max (hi, e);
    }
    return hi <= 0 ? -1.0 : 20 * std::log10 (hi / std::max (lo, 1e-9));
}

static void expect (const char* label, bool ok, double value)
{
    std::printf ("%-28s %8.2f    %s\n", label, value, ok ? "OK" : "FAIL");
    if (! ok) ++failures;
}

int main()
{
    const Buf ch = chords(), sn = snare();
    auto chordData = makeSampleData ("chord_loop", ch.L, ch.R);
    auto snareData = makeSampleData ("snare", sn.L, sn.R);

    LayerBuilder builder;

    // --- Samples mode
    for (int mode = 0; mode < 2; ++mode)
    {
        Engine e; e.prepare (SR, 512);
        std::vector<SourceSpectra> src { { chordData->spectra, 0.8f, 0.6f }, { snareData->spectra, 0.8f, 0.4f } };
        auto r = builder.build (src, { { chordData, 0.8f }, { snareData, 0.8f } }, true, {}, 0, SR);
        std::printf ("Samples: detected key %s\n", keyName (r.key));
        if (r.set == nullptr) { std::printf ("build failed\n"); return 1; }
        delete e.submit (r.set);
        EngineParams p; p.source = 1; p.mode = mode; p.mix = 1.0f; p.key = r.key;
        std::vector<float> L, R;
        render (e, nullptr, 12, p, L, R);
        check (mode ? "samples / wash" : "samples / drone", L, R, (size_t) (3 * SR));
        writeWav (mode ? "samples_wash.wav" : "samples_drone.wav", L, R);
    }

    // --- Return mode: feed a "track", capture after 8 s, then keep playing
    Buf track = ch;
    for (size_t i = 0; i < track.L.size(); ++i) { track.L[i] += sn.L[i] * 0.7f; track.R[i] += sn.R[i] * 0.7f; }
    for (int mode = 0; mode < 2; ++mode)
    {
        Engine e; e.prepare (SR, 512);
        EngineParams p; p.source = 0; p.mode = mode; p.mix = 0.75f;
        std::vector<float> L, R;
        render (e, &track, 8, p, L, R);
        const int win = (int) (2.5 * SR);
        std::vector<float> recent ((size_t) win * 3);
        e.copyRecentInput (recent.data(), win * 3);
        SourceSpectra s;
        for (int k = 0; k < 3; ++k) s.spectra[(size_t) k] = spectrumOf (recent.data() + k * win, win);
        normaliseSet (s.spectra);
        s.level = 1.0f; s.tone = 0.6f;
        auto r = builder.build ({ s }, {}, true, {}, 0, SR);
        std::printf ("Return: detected key %s\n", keyName (r.key));
        if (r.set == nullptr) { std::printf ("build failed\n"); return 1; }
        delete e.submit (r.set);
        p.key = r.key;
        render (e, &track, 10, p, L, R);
        check (mode ? "return / wash" : "return / drone", L, R, (size_t) (11 * SR));
        writeWav (mode ? "return_wash.wav" : "return_drone.wav", L, R);
    }

    // --- in-key modes and noise modes
    std::vector<SourceSpectra> src { { chordData->spectra, 0.8f, 0.6f }, { snareData->spectra, 0.8f, 0.4f } };
    const char* keyLabels[] = { "key mode scale", "key mode chord", "key mode shimmer" };
    const char* noiseLabels[] = { "noise mode air", "noise mode dust", "noise mode wind" };
    for (int km = 0; km < 3; ++km)
    {
        Engine e; e.prepare (SR, 512);
        auto r = builder.build (src, {}, true, {}, km, SR);
        delete e.submit (r.set);
        EngineParams p; p.source = 1; p.key = r.key; p.tonal = 1.0f; p.noise = 0.0f;
        std::vector<float> L, R;
        render (e, nullptr, 6, p, L, R);
        check (keyLabels[km], L, R, (size_t) (3 * SR));
    }
    for (int nm = 0; nm < 3; ++nm)
    {
        Engine e; e.prepare (SR, 512);
        auto r = builder.build (src, {}, true, {}, 0, SR);
        delete e.submit (r.set);
        EngineParams p; p.source = 1; p.key = r.key; p.tonal = 0.0f; p.noise = 1.0f; p.noiseMode = nm;
        std::vector<float> L, R;
        render (e, nullptr, 6, p, L, R);
        check (noiseLabels[nm], L, R, (size_t) (3 * SR));
        if (nm == 1) writeWav ("noise_dust.wav", L, R);
        if (nm == 2) writeWav ("noise_wind.wav", L, R);
    }

    // --- Breath: from the input and from an external sidechain
    const Buf kick = kicks();
    const char* breathLabels[] = { "breath smooth swing dB", "breath pump swing dB", "breath gate swing dB" };
    for (int bm = 0; bm < 3; ++bm)
    {
        Engine e; e.prepare (SR, 512);
        auto r = builder.build (src, {}, true, {}, 0, SR);
        delete e.submit (r.set);
        EngineParams p; p.source = 1; p.key = r.key; p.breath = 1.0f; p.breathMode = bm; p.mix = 1.0f; p.drift = 0.0f;
        std::vector<float> L, R; float md = 0;
        render (e, nullptr, 6, p, L, R, &kick, &md);
        const double sw = swingDb (L, (size_t) (3 * SR));
        expect (breathLabels[bm], sw > (bm == 0 ? 6.0 : 12.0) && md > 0.9f, sw);
        if (bm == 1) writeWav ("breath_pump_sidechain.wav", L, R);
    }
    {
        Engine e; e.prepare (SR, 512);
        auto r = builder.build (src, {}, true, {}, 0, SR);
        delete e.submit (r.set);
        EngineParams p; p.source = 1; p.key = r.key; p.breath = 1.0f; p.breathMode = 1; p.mix = 1.0f; p.drift = 0.0f;
        std::vector<float> L, R; float md = 0;
        Buf quiet = kick; for (auto& v : quiet.L) v *= 0.1f; for (auto& v : quiet.R) v *= 0.1f; // -20 dB send
        render (e, &quiet, 6, p, L, R, nullptr, &md);
        expect ("breath pump at -20 dB input", md > 0.8f, md);
    }

    // --- LFO on level, synced square at 2 Hz
    {
        Engine e; e.prepare (SR, 512);
        auto r = builder.build (src, {}, true, {}, 0, SR);
        delete e.submit (r.set);
        EngineParams p; p.source = 1; p.key = r.key; p.drift = 0.0f; p.breath = 0.0f;
        p.lfoShape = 2; p.lfoHz = 2.0; p.lfoDepth = 1.0f; p.lfoTargets = kModLevel | kModPan;
        std::vector<float> L, R;
        render (e, nullptr, 6, p, L, R);
        const double sw = swingDb (L, (size_t) (3 * SR));
        expect ("lfo level+pan swing dB", sw > 20.0, sw);
        check ("lfo output", L, R, (size_t) (3 * SR));
        for (int shape = 0; shape < 4; ++shape)
        {
            Engine e2; e2.prepare (SR, 512);
            delete e2.submit (builder.build (src, {}, true, {}, 0, SR).set);
            EngineParams q = p; q.lfoShape = shape; q.lfoTargets = kModColor | kModTonal | kModNoise; q.lfoHz = 5.0;
            std::vector<float> L2, R2;
            render (e2, nullptr, 4, q, L2, R2);
            check (shape == 0 ? "lfo sine filter" : shape == 1 ? "lfo triangle filter" : shape == 2 ? "lfo square filter" : "lfo random filter",
                   L2, R2, (size_t) (2 * SR));
        }
    }

    std::printf (failures ? "\n%d check(s) FAILED\n" : "\nAll checks passed\n", failures);
    return failures ? 1 : 0;
}
