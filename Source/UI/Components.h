#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "BenchLookAndFeel.h"
#include "../PluginProcessor.h"

/** Chunky push button with a small indicator lamp. */
class LampButton : public juce::Button
{
public:
    explicit LampButton (const juce::String& name) : juce::Button (name) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

/** Small cream utility button (Load, Clear, Capture). */
class MiniButton : public juce::Button
{
public:
    explicit MiniButton (const juce::String& name) : juce::Button (name) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

/** A row of lamp buttons bound to a choice parameter. */
class ChoiceSwitch : public juce::Component
{
public:
    ChoiceSwitch (juce::RangedAudioParameter& param, const juce::StringArray& labels);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::OwnedArray<LampButton> buttons;
    juce::ParameterAttachment attachment;
};

/** Bakelite knob with engraved label, typewriter readout and hint. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
          const juce::String& labelText, const juce::String& hintText);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    juce::RangedAudioParameter* param;
    juce::String label, hint;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** Horizontal fader for the source panels. */
class Fader : public juce::Slider
{
public:
    Fader()
    {
        setSliderStyle (juce::Slider::LinearHorizontal);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    }
};

/** Draws a Dymo-style embossed tape label. */
void drawDymo (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> maxArea, float rotation);

/** Smooth waveform from min/max peak pairs. */
void drawWaveform (juce::Graphics& g, const std::vector<float>& peaks, juce::Rectangle<float> area, juce::Colour colour);

class SlotComponent : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    SlotComponent (BodenlegerProcessor& p, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void load (const juce::File& f);

    BodenlegerProcessor& proc;
    const int index;
    MiniButton loadButton { "Load" }, clearButton { "Clear" };
    Fader level, air;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAtt, airAtt;
    std::unique_ptr<juce::FileChooser> chooser;
    std::shared_ptr<const bl::SampleData> shown;
    bool dragOver = false;
    juce::String error;
};

class ReturnPanel : public juce::Component
{
public:
    explicit ReturnPanel (BodenlegerProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    BodenlegerProcessor& proc;
    Fader air;
    LampButton follow { "Follow" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> airAtt;
    std::unique_ptr<juce::ButtonParameterAttachment> followAtt;
    std::vector<float> scratch;
    juce::Rectangle<float> scopeArea;
};

class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (BodenlegerProcessor& p);
    void paint (juce::Graphics&) override;
    void updateLive();

private:
    BodenlegerProcessor& proc;
    juce::dsp::FFT fft { 12 };
    std::vector<float> fftData, liveDb, window;
};

class VUMeter : public juce::Component
{
public:
    void setLevel (float rms);
    void paint (juce::Graphics&) override;

private:
    float vu = -20.0f;
};

class KeyButton : public juce::Button
{
public:
    explicit KeyButton (const juce::String& n) : juce::Button (n) {}
    void setState (bool inScale, bool isRoot);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    bool in = false, root = false;
};
