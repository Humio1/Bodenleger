#pragma once

#include "PluginProcessor.h"
#include "UI/Components.h"

/** The whole Bench front panel, drawn at a fixed design size and scaled by the editor. */
class MainPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int kW = 1080, kH = 880;

    explicit MainPanel (BodenlegerProcessor&);
    ~MainPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setParam (const char* id, float denormalisedValue);
    void chooseKey (int root, int scale);
    juce::String statusText() const;
    ModInfo modFor (const char* targetId) const;

    BodenlegerProcessor& proc;

    ChoiceSwitch sourceSwitch, modeSwitch;
    VUMeter vu;
    std::array<std::unique_ptr<SlotComponent>, BodenlegerProcessor::kSlots> slots;
    ReturnPanel returnPanel;
    SpectrumView spectrum;
    MiniButton capture { "Re-roll" };
    LampButton autoKey { "Auto" };
    std::unique_ptr<juce::ButtonParameterAttachment> autoAtt;
    juce::ComboBox scaleBox;
    juce::OwnedArray<KeyButton> keys;
    juce::OwnedArray<Knob> knobs;

    // character switches
    ToggleSwitch keyModeSwitch, noiseModeSwitch, breathModeSwitch, listenSwitch;

    // modulation
    ChoiceSwitch lfoShapes;
    LampButton lfoSync { "Sync to tempo" };
    std::unique_ptr<juce::ButtonParameterAttachment> syncAtt;
    Knob rateKnob, divisionKnob, depthKnob;
    juce::OwnedArray<LampButton> targets;
    juce::OwnedArray<juce::ButtonParameterAttachment> targetAtts;
    float shownLfo = 0.0f;
    juce::TooltipWindow tooltips { this, 700 };

    int shownSource = -1;
    bool shownBuilding = false;
    juce::String shownStatus;
    int tick = 0;

    juce::Rectangle<int> statusArea, buildingArea, rackTitleArea, lfoLampArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};

class BodenlegerEditor : public juce::AudioProcessorEditor
{
public:
    explicit BodenlegerEditor (BodenlegerProcessor&);
    ~BodenlegerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    BodenlegerProcessor& proc;
    BenchLookAndFeel lnf;
    MainPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BodenlegerEditor)
};
