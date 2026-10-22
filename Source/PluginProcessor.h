#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Engine/Builder.h"

namespace ids
{
inline constexpr const char* source    = "source";
inline constexpr const char* mode      = "mode";
inline constexpr const char* keyAuto   = "keyAuto";
inline constexpr const char* keyRoot   = "keyRoot";
inline constexpr const char* keyScale  = "keyScale";
inline constexpr const char* tonal     = "tonal";
inline constexpr const char* noise     = "noise";
inline constexpr const char* drift     = "drift";
inline constexpr const char* color     = "color";
inline constexpr const char* breath    = "breath";
inline constexpr const char* mix       = "mix";
inline constexpr const char* returnAir = "returnAir";
inline constexpr const char* follow    = "follow";
inline constexpr const char* keyMode     = "keyMode";
inline constexpr const char* noiseMode   = "noiseMode";
inline constexpr const char* breathMode  = "breathMode";
inline constexpr const char* breathListen = "breathListen";
inline constexpr const char* lfoShape    = "lfoShape";
inline constexpr const char* lfoSync     = "lfoSync";
inline constexpr const char* lfoRate     = "lfoRate";
inline constexpr const char* lfoDivision = "lfoDivision";
inline constexpr const char* lfoDepth    = "lfoDepth";
inline constexpr const char* modColor    = "modColor";
inline constexpr const char* modTonal    = "modTonal";
inline constexpr const char* modNoise    = "modNoise";
inline constexpr const char* modLevel    = "modLevel";
inline constexpr const char* modPan      = "modPan";
inline juce::String slotLevel (int i) { return "slot" + juce::String (i + 1) + "Level"; }
inline juce::String slotAir (int i)   { return "slot" + juce::String (i + 1) + "Air"; }
} // namespace ids

struct DisplayData
{
    std::array<bl::Snapshot, bl::kSnapshots> snapshots;
    std::vector<float> mask;
    bl::Key key;
    double sampleRate = 48000.0;
    int sourceCount = 0;
};

class BodenlegerProcessor : public juce::AudioProcessor,
                            private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr int kSlots = 4;

    BodenlegerProcessor();
    ~BodenlegerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Bodenleger"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- used by the editor
    juce::AudioProcessorValueTreeState apvts;
    bl::Engine engine;

    bool loadSample (int slot, const juce::File& file);
    void clearSample (int slot);
    std::shared_ptr<const bl::SampleData> getSample (int slot) const;
    void requestBuild (bool immediate);
    std::shared_ptr<const DisplayData> getDisplay() const;
    bl::Key getEffectiveKey() const;
    bl::Key getDetectedKey() const { return { detectedRoot.load(), detectedScale.load() }; }
    bool isBuilding() const { return building.load(); }
    int getSourceMode() const { return (int) apvts.getRawParameterValue (ids::source)->load(); }
    bool isAudioRunning() const;
    int getKeyMode() const { return (int) apvts.getRawParameterValue (ids::keyMode)->load(); }
    /** True while the host feeds the sidechain input with something audible. */
    bool isSidechainActive() const { return juce::Time::getMillisecondCounter() - lastSidechainTime.load() < 600; }
    float getLfoDepthFor (const char* targetId) const;

private:
    class BuildThread;
    friend class BuildThread;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String& id, float value) override;
    void doBuild();
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    juce::AudioFormatManager formats;
    mutable juce::CriticalSection slotLock;
    std::array<std::shared_ptr<const bl::SampleData>, kSlots> slots;
    std::array<juce::File, kSlots> slotFiles;
    std::array<double, kSlots> slotRates {};

    mutable juce::SpinLock displayLock;
    std::shared_ptr<const DisplayData> display;

    bl::LayerBuilder builder;
    std::unique_ptr<BuildThread> buildThread;
    std::atomic<bool> buildRequested { true }, forceBuild { true }, building { false };
    std::atomic<juce::uint32> requestTime { 0 }, lastBuildTime { 0 }, lastAudioTime { 0 };
    std::atomic<int> detectedRoot { 9 }, detectedScale { 0 };
    std::atomic<double> hostRate { 48000.0 };
    std::atomic<juce::uint32> lastSidechainTime { 0 };
    juce::AudioBuffer<float> sidechainCopy;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BodenlegerProcessor)
};
