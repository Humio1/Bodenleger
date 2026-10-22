#pragma once
// Real-time side of Bodenleger. Plays the resynthesised pad layers, runs the Wash grains,
// resonators, reverb, colour filters and breath ducking. No allocations or locks on the audio thread:
// new layer sets arrive through an atomic hand-off and old ones go back through a retire queue.

#include "Spectral.h"
#include "DSP.h"
#include <atomic>
#include <memory>
#include <string>

namespace bl
{
struct SampleData
{
    std::string name;
    std::vector<float> left, right, mono;
    std::array<Spec, kSnapshots> spectra;
    std::vector<float> peaks; // min/max pairs for drawing
};

struct LayerSet
{
    int length = 0;
    std::array<StereoLoop, kSnapshots> raw, inKey, noise;
    std::vector<std::pair<std::shared_ptr<const SampleData>, float>> grainSources; // Samples mode
};

enum LfoTarget { kModColor = 1, kModTonal = 2, kModNoise = 4, kModLevel = 8, kModPan = 16 };

struct EngineParams
{
    int mode = 0;   // 0 = Drone, 1 = Wash
    int source = 0; // 0 = Return, 1 = Samples
    float tonal = 0.6f, noise = 0.3f, drift = 0.4f, color = 0.5f, breath = 0.3f, mix = 1.0f;
    Key key;

    int noiseMode = 0;  // 0 = Air, 1 = Dust, 2 = Wind
    int breathMode = 0; // 0 = Smooth, 1 = Pump, 2 = Gate
    const float* sidechainL = nullptr; // when set, Breath listens to this instead of the input
    const float* sidechainR = nullptr;

    int lfoShape = 0;        // 0 = Sine, 1 = Triangle, 2 = Square, 3 = Random
    double lfoHz = 0.25;
    double lfoPhase = -1.0;  // >= 0: lock the phase at the block start (tempo sync)
    float lfoDepth = 0.0f;
    int lfoTargets = 0;      // LfoTarget bits
};

class Engine
{
public:
    Engine();
    ~Engine();

    void prepare (double sampleRate, int maxBlockSize);
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const EngineParams& p) noexcept;

    /** Called from the builder thread. Returns a previously pending set that was never picked up (caller deletes it). */
    LayerSet* submit (LayerSet* set) noexcept;
    /** Called from the builder thread. Returns one set the audio thread has finished with, or nullptr. */
    LayerSet* takeRetired() noexcept;

    /** Mono copy of the most recent n input samples. Returns how many samples have ever been written. */
    int64_t copyRecentInput (float* dest, int n) const noexcept;
    int64_t inputWritten() const noexcept { return written.load (std::memory_order_acquire); }
    void copyRecentOutput (float* dest, int n) const noexcept;

    double getSampleRate() const noexcept { return sr; }
    bool hasLayers() const noexcept { return hasCurrent.load(); }

    std::atomic<float> outputRms { 0.0f };
    std::atomic<float> duckAmount { 0.0f }; // 0..1, how much Breath is pulling the pad down
    std::atomic<float> lfoValue { 0.0f };   // -1..1
    std::atomic<float> snapshotWeight[kSnapshots];

private:
    struct Play
    {
        LayerSet* set = nullptr;
        int pos[kSnapshots] {};
        double tonPhase[kSnapshots] {};
    };

    struct Grain
    {
        bool active = false;
        const SampleData* sample = nullptr; // nullptr = input ring
        double start = 0, pos = 0, rate = 1;
        int length = 0, age = 0;
        float amp = 0.22f, gainL = 1, gainR = 1;
    };

    void retire (LayerSet* s) noexcept;
    void renderLayers (Play& pl, float fadeGain, const float* snapGain, float gr, float gt, float gn,
                       float tonRate[kSnapshots], float& L, float& R, float& NL, float& NR) noexcept;
    float lfoShapeValue (int shape, double phase) const noexcept;
    void spawnGrain (const EngineParams& p) noexcept;
    float nextRandom() noexcept;
    void updateResonators (Key k);

    double sr = 48000.0;

    std::atomic<LayerSet*> pending { nullptr };
    static constexpr int kRetireSlots = 16;
    std::atomic<LayerSet*> retired[kRetireSlots];
    Play cur, prev;
    float fade = 1.0f, fadeStep = 0.0f;
    std::atomic<bool> hasCurrent { false };

    // input ring (for Return mode analysis and grains)
    std::vector<float> ringL, ringR;
    int64_t ringMask = 0;
    std::atomic<int64_t> written { 0 };

    // output ring for the spectrum display
    static constexpr int kOutRing = 8192;
    std::vector<float> outRing;
    std::atomic<int> outWrite { 0 };

    // drift
    double driftPhase = 0;
    Smoothed snapGain[kSnapshots];

    // grains
    static constexpr int kMaxGrains = 40;
    Grain grains[kMaxGrains];
    double grainCountdown = 0;
    uint32_t rngState = 0x9E3779B9u;

    // filters and buses
    Biquad grainHP[2], res[5][2], shelfLo[2], shelfHi[2], lp[2];
    Key resKey { -1, -1 };
    FDNReverb reverb;
    Smoothed layerLevel, directLevel, revSend, grainLevel, grainDry, resLevel;
    Smoothed gRaw, gTon, gNoi, colour, dryMix, wetMix, duck;
    float detEnv = 0;
    int currentOffset = 0;

    // noise modes
    Smoothed wAir, wDust, wWind;
    float crackle[2] {};
    Biquad windBp[2][2];
    double windPhase = 0, windTarget = 0.5, windPos = 0.5;

    // LFO
    double lfoPh = 0;
    float lfoRandPrev = 0, lfoRandNext = 0, lfoSmooth = 0;
    Smoothed panL, panR, levelMod;
    int filterCounter = 0;
};
} // namespace bl
