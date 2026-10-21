#pragma once
// "Bench" look: a worn piece of vintage studio hardware. Cream enamel, bakelite knobs,
// engraved labels, typewriter readouts and Dymo tape.

#include <juce_gui_extra/juce_gui_extra.h>

namespace bench
{
namespace col
{
const juce::Colour page    { 0xff191613 };
const juce::Colour cream   { 0xffe2d4b8 };
const juce::Colour cream2  { 0xffefe5d1 };
const juce::Colour paper   { 0xfff1e7d2 };
const juce::Colour well    { 0xffcfbe9d };
const juce::Colour line    { 0xffb3a284 };
const juce::Colour ink     { 0xff2a1f16 };
const juce::Colour ink2    { 0xff56463a };
const juce::Colour ink3    { 0xff806d58 };
const juce::Colour engrave { 0xff6f5c48 };
const juce::Colour accent  { 0xffb8381c };
const juce::Colour dark    { 0xff2b241d };
const juce::Colour lit     { 0xffffd9a3 };
const juce::Colour lampOn  { 0xffff6a3a };
const juce::Colour lampOff { 0xff4d2a1c };
} // namespace col

juce::Font script (float height);
juce::Font label (float height, float kerning = 0.14f);
juce::Font typewriter (float height);
juce::Font tape (float height);

/** Draws a fine film-grain texture over an area. */
void drawGrain (juce::Graphics& g, juce::Rectangle<int> area, float opacity);
void drawScrew (juce::Graphics& g, juce::Point<float> centre, float slotAngle);
/** Uppercase engraved text: a light highlight under dark ink. */
void drawEngraved (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                   const juce::Font& font, juce::Justification just, juce::Colour colour = col::engrave);
/** Rounded recess with a dark bezel and a thin chrome outline. */
void drawBezel (juce::Graphics& g, juce::Rectangle<float> r, float corner);
} // namespace bench

class BenchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BenchLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
};
