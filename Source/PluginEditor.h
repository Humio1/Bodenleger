#pragma once

#include "PluginProcessor.h"
#include "UI/Components.h"

class BodenlegerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BodenlegerEditor (BodenlegerProcessor&);
    ~BodenlegerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setParam (const char* id, float denormalisedValue);
    void chooseKey (int root, int scale);
    juce::String statusText() const;

    BodenlegerProcessor& proc;
    BenchLookAndFeel lnf;

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

    int shownSource = -1;
    bool shownBuilding = false;
    juce::String shownStatus;
    int tick = 0;

    juce::Rectangle<int> statusArea, buildingArea, rackTitleArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BodenlegerEditor)
};
