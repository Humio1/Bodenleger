#include "Engine.h"
#include <cstring>

namespace bl
{
Engine::Engine()
{
    for (auto& r : retired) r.store (nullptr);
    for (auto& w : snapshotWeight) w.store (1.0f);
}

Engine::~Engine()
{
    delete pending.exchange (nullptr);
    delete cur.set;
    delete prev.set;
    for (auto& r : retired) delete r.exchange (nullptr);
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;

    int64_t size = 1;
    while (size < (int64_t) (16.0 * sr)) size <<= 1;
    ringL.assign ((size_t) size, 0.0f);
    ringR.assign ((size_t) size, 0.0f);
    ringMask = size - 1;
    written.store (0);
    outRing.assign ((size_t) kOutRing, 0.0f);
    outWrite.store (0);

    for (auto& s : snapGain) { s.setTime (sr, 0.2); s.snap (0.6f); }
    for (auto* s : { &layerLevel, &directLevel, &revSend, &grainLevel }) s->setTime (sr, 0.25);
    for (auto* s : { &grainDry, &resLevel, &gRaw, &gTon, &gNoi, &colour }) s->setTime (sr, 0.012);
    for (auto* s : { &dryMix, &wetMix, &wAir, &wDust, &wWind }) s->setTime (sr, 0.03);
    for (auto* s : { &panL, &panR, &levelMod }) s->setTime (sr, 0.004);
    duck.setTime (sr, 0.0015);
    layerLevel.snap (1); directLevel.snap (1); revSend.snap (0.22f); grainLevel.snap (0);
    duck.snap (1); colour.snap (0.5f); dryMix.snap (0); wetMix.snap (1);
    wAir.snap (1); wDust.snap (0); wWind.snap (0);
    panL.snap (1); panR.snap (1); levelMod.snap (1);
    for (auto& ch : windBp) for (auto& f : ch) f.reset();
    crackle[0] = crackle[1] = 0;
    lfoPh = 0; lfoSmooth = 0; lfoRandPrev = lfoRandNext = 0;

    for (auto& g : grains) g.active = false;
    for (auto& f : grainHP) { f.highpass (sr, 160.0, 0.707); f.reset(); }
    for (auto& r : res) for (auto& f : r) f.reset();
    for (auto* f : { &shelfLo[0], &shelfLo[1], &shelfHi[0], &shelfHi[1], &lp[0], &lp[1] }) f->reset();
    resKey = { -1, -1 };
    reverb.prepare (sr, 6.0);
    detEnv = 0;
    filterCounter = 0;
    grainCountdown = 0;
}

LayerSet* Engine::submit (LayerSet* set) noexcept
{
    return pending.exchange (set, std::memory_order_acq_rel);
}

LayerSet* Engine::takeRetired() noexcept
{
    for (auto& r : retired)
        if (auto* p = r.exchange (nullptr, std::memory_order_acq_rel))
            return p;
    return nullptr;
}

void Engine::retire (LayerSet* s) noexcept
{
    if (s == nullptr) return;

    for (auto& g : grains)
        if (g.active && g.sample != nullptr)
            for (auto& src : s->grainSources)
                if (src.first.get() == g.sample) { g.active = false; break; }

    for (auto& r : retired)
    {
        LayerSet* expected = nullptr;
        if (r.compare_exchange_strong (expected, s, std::memory_order_acq_rel))
            return;
    }
    // Retire queue full (should never happen): leak rather than free on the audio thread.
}

int64_t Engine::copyRecentInput (float* dest, int n) const noexcept
{
    const int64_t w = written.load (std::memory_order_acquire);
    if (ringL.empty()) { std::memset (dest, 0, sizeof (float) * (size_t) n); return w; }
    for (int i = 0; i < n; ++i)
    {
        const int64_t a = w - n + i;
        dest[i] = a < 0 ? 0.0f : 0.5f * (ringL[(size_t) (a & ringMask)] + ringR[(size_t) (a & ringMask)]);
    }
    return w;
}

void Engine::copyRecentOutput (float* dest, int n) const noexcept
{
    n = std::min (n, kOutRing);
    const int w = outWrite.load (std::memory_order_acquire);
    if (outRing.empty()) { std::memset (dest, 0, sizeof (float) * (size_t) n); return; }
    for (int i = 0; i < n; ++i)
        dest[i] = outRing[(size_t) ((w - n + i) & (kOutRing - 1))];
}

float Engine::nextRandom() noexcept
{
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return (rngState >> 8) * (1.0f / 16777216.0f);
}

void Engine::updateResonators (Key k)
{
    if (k == resKey) return;
    resKey = k;
    const auto sc = scaleIntervals (k.scale);
    int iv[5];
    if (sc.size() == 7) { iv[0] = 0; iv[1] = sc[2]; iv[2] = sc[4]; iv[3] = sc[6]; iv[4] = 12; }
    else { iv[0] = 0; iv[1] = 7; iv[2] = 12; iv[3] = 19; iv[4] = 24; }
    for (int j = 0; j < 5; ++j)
    {
        const double f = 440.0 * std::pow (2.0, (48 + k.root + iv[j] - 69) / 12.0);
        for (auto& ch : res[j]) ch.bandpass (sr, f, 28.0);
    }
}

void Engine::spawnGrain (const EngineParams& p) noexcept
{
    int slot = -1;
    for (int i = 0; i < kMaxGrains; ++i)
        if (! grains[i].active) { slot = i; break; }
    if (slot < 0) return;

    static const double rates[] = { 1.0, 1.0, 0.5, 0.5, 2.0, 1.4983 };
    Grain g;
    g.length = (int) ((0.7 + nextRandom() * 1.1) * sr);
    g.rate = rates[(int) (nextRandom() * 5.999f)];
    const float pan = nextRandom() * 1.6f - 0.8f;
    g.gainL = std::min (1.0f, 1.0f - pan);
    g.gainR = std::min (1.0f, 1.0f + pan);

    if (p.source == 0)
    {
        const int64_t w = written.load (std::memory_order_relaxed) + currentOffset;
        const double back = (0.4 + nextRandom() * 3.0) * sr + g.length * g.rate + 8.0;
        if (back > (double) w || back > (double) (ringMask + 1) - sr) return;
        g.sample = nullptr;
        g.start = (double) w - back;
    }
    else
    {
        if (cur.set == nullptr || cur.set->length <= 1 || cur.set->grainSources.empty()) return;
        float total = 0;
        for (auto& s : cur.set->grainSources) total += s.second;
        if (total <= 0.0001f) return;
        float r = nextRandom() * total;
        const SampleData* pick = cur.set->grainSources.front().first.get();
        for (auto& s : cur.set->grainSources)
        {
            r -= s.second;
            if (r <= 0) { pick = s.first.get(); break; }
        }
        if (pick == nullptr || pick->left.size() < 2) return;
        g.sample = pick;
        g.start = nextRandom() * std::max (1.0, (double) pick->left.size() - 0.1 * sr);
    }
    g.pos = 0;
    g.age = 0;
    g.active = true;
    grains[slot] = g;
}

float Engine::lfoShapeValue (int shape, double ph) const noexcept
{
    switch (shape)
    {
        case 1:  return (float) (1.0 - 4.0 * std::abs (ph - 0.5));           // triangle, starts low
        case 2:  return ph < 0.5 ? 1.0f : -1.0f;                             // square
        case 3:  return lfoRandPrev + (lfoRandNext - lfoRandPrev)            // random, glides over the first quarter
                        * (float) std::min (1.0, ph * 4.0);
        default: return (float) std::sin (2.0 * kPi * ph);                  // sine
    }
}

void Engine::renderLayers (Play& pl, float fadeGain, const float* sg, float gr, float gt, float gn,
                           float tonRate[kSnapshots], float& L, float& R, float& NL, float& NR) noexcept
{
    const LayerSet& s = *pl.set;
    const int len = s.length;
    if (len <= 1) return; // an empty set means silence
    for (int k = 0; k < kSnapshots; ++k)
    {
        const int i = pl.pos[k];
        const int t0 = (int) pl.tonPhase[k];
        const float fr = (float) (pl.tonPhase[k] - t0);
        const int t1 = t0 + 1 >= len ? 0 : t0 + 1;
        const auto& raw = s.raw[(size_t) k];
        const auto& ton = s.inKey[(size_t) k];
        const auto& noi = s.noise[(size_t) k];

        const float tl = ton.left[(size_t) t0] + (ton.left[(size_t) t1] - ton.left[(size_t) t0]) * fr;
        const float tr = ton.right[(size_t) t0] + (ton.right[(size_t) t1] - ton.right[(size_t) t0]) * fr;
        const float g = sg[k] * fadeGain;
        L += g * (gr * raw.left[(size_t) i] + gt * tl);
        R += g * (gr * raw.right[(size_t) i] + gt * tr);
        NL += g * gn * noi.left[(size_t) i];
        NR += g * gn * noi.right[(size_t) i];

        if (++pl.pos[k] >= len) pl.pos[k] = 0;
        pl.tonPhase[k] += tonRate[k];
        if (pl.tonPhase[k] >= len) pl.tonPhase[k] -= len;
    }
}

void Engine::process (const float* inL, const float* inR, float* outL, float* outR, int n, const EngineParams& p) noexcept
{
    // --- pick up a freshly built layer set
    if (auto* fresh = pending.exchange (nullptr, std::memory_order_acq_rel))
    {
        if (prev.set != nullptr) retire (prev.set);
        prev = cur;
        cur = Play {};
        cur.set = fresh;
        for (int k = 0; k < kSnapshots; ++k)
        {
            cur.pos[k] = (int) (nextRandom() * std::max (0, fresh->length - 1));
            cur.tonPhase[k] = nextRandom() * std::max (0, fresh->length - 1);
        }
        fade = 0.0f;
        fadeStep = (float) (1.0 / (2.5 * sr));
        hasCurrent.store (true);
    }
    if (fade >= 1.0f && prev.set != nullptr)
    {
        retire (prev.set);
        prev = Play {};
    }

    // --- block-rate parameters
    const float d = p.drift;
    const double rate = 0.02 + d * 0.13;
    driftPhase += n / sr * rate * 2.0 * kPi;
    if (driftPhase > 1000.0 * kPi) driftPhase -= 1000.0 * kPi;
    float tonRate[kSnapshots];
    for (int k = 0; k < kSnapshots; ++k)
    {
        const double w = 0.5 + 0.5 * std::sin (driftPhase + k * 2.094);
        const float g = (float) ((1.0 - d) + d * 2.0 * w * w);
        snapGain[k].target = g * 0.6f;
        snapshotWeight[k].store (g, std::memory_order_relaxed);
        tonRate[k] = (float) (1.0 + d * 0.004 * std::sin (driftPhase * 0.7 + k));
    }

    const bool wash = p.mode == 1;
    layerLevel.target  = wash ? 0.45f : 1.0f;
    directLevel.target = wash ? 0.55f : 1.0f;
    revSend.target     = wash ? 0.95f : 0.22f;
    grainLevel.target  = wash ? 1.0f : 0.0f;
    dryMix.target = (float) std::cos (p.mix * kPi / 2);
    wetMix.target = (float) std::sin (p.mix * kPi / 2);
    wAir.target  = p.noiseMode == 0 ? 1.0f : 0.0f;
    wDust.target = p.noiseMode == 1 ? 1.0f : 0.0f;
    wWind.target = p.noiseMode == 2 ? 1.0f : 0.0f;
    updateResonators (p.key);

    // Breath detector: Smooth follows the energy, Pump hits hard and lets go fast, Gate chops.
    struct DetectorShape { double attack, release; float lowDb, highDb, curve; };
    static const DetectorShape shapes[3] = { { 0.012, 0.40, -42.0f, -6.0f, 1.0f },
                                             { 0.0015, 0.16, -44.0f, -18.0f, 0.5f },
                                             { 0.0005, 0.045, -34.0f, -29.0f, 1.0f } };
    const auto& det = shapes[std::clamp (p.breathMode, 0, 2)];
    const float attK = (float) (1.0 - std::exp (-1.0 / (det.attack * sr)));
    const float relK = (float) (1.0 - std::exp (-1.0 / (det.release * sr)));
    const double wobble = 1.0 + d * 0.25 * std::sin (driftPhase * 0.5);

    // LFO
    const int targets = p.lfoDepth > 0.0005f ? p.lfoTargets : 0;
    const float depth = p.lfoDepth;
    const double lfoInc = std::clamp (p.lfoHz, 0.001, 40.0) / sr;
    if (p.lfoPhase >= 0.0)
    {
        double diff = p.lfoPhase - lfoPh;
        diff -= std::floor (diff + 0.5);
        if (std::abs (diff) > 0.02) lfoPh = p.lfoPhase; // jump after a relocate, otherwise stay continuous
    }
    const float lfoK = (float) (1.0 - std::exp (-1.0 / (0.0015 * sr)));

    // wind sweep target (slow random walk)
    windPhase += n / sr * 0.12;
    if (windPhase >= 1.0) { windPhase -= 1.0; windTarget = nextRandom(); }
    float maxDuck = 0.0f;

    const int64_t w0 = written.load (std::memory_order_relaxed);
    int ow = outWrite.load (std::memory_order_relaxed);
    double vuSum = 0;

    for (int i = 0; i < n; ++i)
    {
        const float xl = inL[i], xr = inR[i];
        currentOffset = i;

        // input ring
        const int64_t a = (w0 + i) & ringMask;
        ringL[(size_t) a] = xl;
        ringR[(size_t) a] = xr;

        // LFO
        const double prevPh = lfoPh;
        lfoPh += lfoInc;
        if (lfoPh >= 1.0) lfoPh -= std::floor (lfoPh);
        if (lfoPh < prevPh) { lfoRandPrev = lfoRandNext; lfoRandNext = nextRandom() * 2.0f - 1.0f; }
        lfoSmooth += (lfoShapeValue (p.lfoShape, lfoPh) - lfoSmooth) * lfoK;
        const float lfo = lfoSmooth;
        auto modded = [&] (float base, int bit) { return (targets & bit) ? std::clamp (base + 0.5f * depth * lfo, 0.0f, 1.0f) : base; };
        const float tonal = modded (p.tonal, kModTonal);
        const float noise = modded (p.noise, kModNoise);
        colour.target = modded (p.color, kModColor);
        gRaw.target = (float) std::cos (tonal * kPi / 2);
        gTon.target = (float) std::sin (tonal * kPi / 2) * 1.1f;
        gNoi.target = noise * noise * 0.9f;
        grainDry.target = 1.0f - tonal * 0.85f;
        resLevel.target = tonal * 6.0f;
        levelMod.target = (targets & kModLevel) ? 1.0f - depth * (0.5f - 0.5f * lfo) : 1.0f;
        const float pan = (targets & kModPan) ? depth * lfo : 0.0f;
        panL.target = std::min (1.0f, 1.0f - pan);
        panR.target = std::min (1.0f, 1.0f + pan);

        // breath detector (input or external sidechain)
        const float key = p.sidechainL != nullptr ? 0.5f * (std::abs (p.sidechainL[i]) + std::abs (p.sidechainR[i]))
                                                  : 0.5f * (std::abs (xl) + std::abs (xr));
        detEnv += (key - detEnv) * (key > detEnv ? attK : relK);
        const float db = 20.0f * std::log10 (detEnv + 1e-9f);
        float amt = std::clamp ((db - det.lowDb) / (det.highDb - det.lowDb), 0.0f, 1.0f);
        amt = amt * amt * (3.0f - 2.0f * amt);
        if (det.curve < 1.0f) amt = std::sqrt (amt);
        const float pull = p.breath * amt;
        maxDuck = std::max (maxDuck, pull);
        duck.target = 1.0f - pull;

        // colour filters (coefficients every 32 samples)
        const float c = colour.next();
        if ((filterCounter++ & 31) == 0)
        {
            windPos += (windTarget - windPos) * 0.004;
            const double wc = 260.0 * std::pow (10.0, windPos * 1.05);
            windBp[0][0].bandpass (sr, wc, 3.2);         windBp[0][1].bandpass (sr, wc * 2.3, 4.0);
            windBp[1][0].bandpass (sr, wc * 1.18, 3.2);  windBp[1][1].bandpass (sr, wc * 2.6, 4.0);
            const double cut = 600.0 * std::pow (30.0, c) * wobble;
            for (int ch = 0; ch < 2; ++ch)
            {
                lp[ch].lowpass (sr, cut, 0.5);
                shelfLo[ch].lowShelf (sr, 220.0, (0.5 - c) * 10.0);
                shelfHi[ch].highShelf (sr, 4000.0, (c - 0.5) * 12.0);
            }
        }

        // pad layers
        float sg[kSnapshots];
        for (int k = 0; k < kSnapshots; ++k) sg[k] = snapGain[k].next();
        const float gr = gRaw.next(), gt = gTon.next(), gn = gNoi.next();
        float L = 0, R = 0, NL = 0, NR = 0;
        if (cur.set != nullptr) renderLayers (cur, std::sqrt (fade), sg, gr, gt, gn, tonRate, L, R, NL, NR);
        if (prev.set != nullptr) renderLayers (prev, std::sqrt (1.0f - fade), sg, gr, gt, gn, tonRate, L, R, NL, NR);
        fade = std::min (1.0f, fade + fadeStep);

        // noise modes: Air as is, Dust crackles, Wind sweeps through resonant bands
        {
            const float wa = wAir.next(), wd = wDust.next(), ww = wWind.next();
            float outNL = NL * wa, outNR = NR * wa;
            if (wd > 0.0005f)
            {
                const float rate = (float) (38.0 / sr);
                for (int ch = 0; ch < 2; ++ch)
                {
                    if (nextRandom() < rate) crackle[ch] = std::max (crackle[ch], 0.35f + 0.65f * nextRandom());
                    crackle[ch] *= 0.9965f;
                }
                outNL += wd * NL * (0.1f + crackle[0] * 3.2f);
                outNR += wd * NR * (0.1f + crackle[1] * 3.2f);
            }
            if (ww > 0.0005f)
            {
                const float bl = windBp[0][0].process (NL) + 0.6f * windBp[0][1].process (NL);
                const float br = windBp[1][0].process (NR) + 0.6f * windBp[1][1].process (NR);
                outNL += ww * (NL * 0.2f + bl * 2.6f);
                outNR += ww * (NR * 0.2f + br * 2.6f);
            }
            L += outNL;
            R += outNR;
        }

        // grains
        if (wash)
        {
            grainCountdown -= 1.0;
            if (grainCountdown <= 0)
            {
                spawnGrain (p);
                grainCountdown = (0.09 + nextRandom() * 0.08) * sr;
            }
        }
        float gl = 0, gR = 0;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            const double at = g.start + g.pos;
            float sl, sr2;
            if (g.sample == nullptr)
            {
                const int64_t i0 = (int64_t) at;
                const float f = (float) (at - (double) i0);
                const size_t ia = (size_t) (i0 & ringMask), ib = (size_t) ((i0 + 1) & ringMask);
                sl  = ringL[ia] + (ringL[ib] - ringL[ia]) * f;
                sr2 = ringR[ia] + (ringR[ib] - ringR[ia]) * f;
            }
            else
            {
                const auto& sL = g.sample->left;
                const auto& sR = g.sample->right;
                const double len = (double) sL.size();
                double pos = std::fmod (at, len);
                const size_t ia = (size_t) pos, ib = ia + 1 >= sL.size() ? 0 : ia + 1;
                const float f = (float) (pos - (double) ia);
                sl  = sL[ia] + (sL[ib] - sL[ia]) * f;
                sr2 = sR[ia] + (sR[ib] - sR[ia]) * f;
            }
            const float up = 0.45f * g.length;
            const float e = g.age < up ? g.amp * g.age / up : g.amp * (g.length - g.age) / (g.length - up);
            gl += sl * e * g.gainL;
            gR += sr2 * e * g.gainR;
            g.pos += g.rate;
            if (++g.age >= g.length) g.active = false;
        }
        const float glev = grainLevel.next();
        const float hl = grainHP[0].process (gl * glev), hr = grainHP[1].process (gR * glev);
        const float gd = grainDry.next(), rl = resLevel.next();
        float resL = 0, resR = 0;
        for (int j = 0; j < 5; ++j) { resL += res[j][0].process (hl); resR += res[j][1].process (hr); }
        const float grainOutL = hl * gd + resL * rl;
        const float grainOutR = hr * gd + resR * rl;

        // pad bus → direct + reverb → colour → breath → mix
        const float ll = layerLevel.next();
        const float padL = L * ll + grainOutL, padR = R * ll + grainOutR;
        const float rs = revSend.next();
        float vl, vr;
        reverb.process (padL * rs, padR * rs, vl, vr);
        const float dl = directLevel.next();
        float yl = padL * dl + vl, yr = padR * dl + vr;
        yl = lp[0].process (shelfHi[0].process (shelfLo[0].process (yl)));
        yr = lp[1].process (shelfHi[1].process (shelfLo[1].process (yr)));
        const float dk = duck.next() * levelMod.next();
        yl = std::tanh (yl) * dk * panL.next();
        yr = std::tanh (yr) * dk * panR.next();

        const float dm = dryMix.next(), wm = wetMix.next();
        const float ol = xl * dm + yl * wm, orr = xr * dm + yr * wm;
        outL[i] = ol;
        outR[i] = orr;

        outRing[(size_t) (ow & (kOutRing - 1))] = 0.5f * (ol + orr);
        ++ow;
        vuSum += 0.5 * ((double) ol * ol + (double) orr * orr);
    }

    written.store (w0 + n, std::memory_order_release);
    outWrite.store (ow & (kOutRing - 1), std::memory_order_release);
    outputRms.store ((float) std::sqrt (vuSum / std::max (1, n)), std::memory_order_relaxed);
    duckAmount.store (maxDuck, std::memory_order_relaxed);
    lfoValue.store (lfoSmooth, std::memory_order_relaxed);
}
} // namespace bl
