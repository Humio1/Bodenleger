#include "BenchLookAndFeel.h"
#include "BinaryData.h"

namespace bench
{
namespace
{
struct Typefaces
{
    juce::Typeface::Ptr script, oswald, typewriter, tape;

    Typefaces()
    {
        script     = juce::Typeface::createSystemTypefaceFor (BinaryData::YellowtailRegular_ttf, (size_t) BinaryData::YellowtailRegular_ttfSize);
        oswald     = juce::Typeface::createSystemTypefaceFor (BinaryData::OswaldVariable_ttf, (size_t) BinaryData::OswaldVariable_ttfSize);
        typewriter = juce::Typeface::createSystemTypefaceFor (BinaryData::SpecialEliteRegular_ttf, (size_t) BinaryData::SpecialEliteRegular_ttfSize);
        tape       = juce::Typeface::createSystemTypefaceFor (BinaryData::CourierPrimeBold_ttf, (size_t) BinaryData::CourierPrimeBold_ttfSize);
    }

    static Typefaces& get()
    {
        static Typefaces t;
        return t;
    }
};

juce::Font make (const juce::Typeface::Ptr& tf, float h, float kerning = 0.0f)
{
    return juce::Font (juce::FontOptions (tf).withHeight (h).withKerningFactor (kerning));
}
} // namespace

juce::Font script (float h)                 { return make (Typefaces::get().script, h); }
juce::Font label (float h, float kerning)   { return make (Typefaces::get().oswald, h, kerning); }
juce::Font typewriter (float h)             { return make (Typefaces::get().typewriter, h); }
juce::Font tape (float h)                   { return make (Typefaces::get().tape, h, 0.1f); }

static const juce::Image& grainImage()
{
    static const juce::Image img = []
    {
        juce::Image im (juce::Image::ARGB, 256, 256, true);
        juce::Random rnd (147);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x)
            {
                const float r = (rnd.nextFloat() + rnd.nextFloat() + rnd.nextFloat()) / 3.0f - 0.5f;
                const auto base = r > 0 ? juce::Colours::white : juce::Colour (0xff2a1a0a);
                im.setPixelAt (x, y, base.withAlpha (juce::jlimit (0.0f, 1.0f, std::abs (r) * 0.55f)));
            }
        return im;
    }();
    return img;
}

void drawGrain (juce::Graphics& g, juce::Rectangle<int> area, float opacity)
{
    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (area);
    g.setOpacity (opacity);
    g.setTiledImageFill (grainImage(), 0, 0, opacity);
    g.fillRect (area);
}

void drawScrew (juce::Graphics& g, juce::Point<float> c, float angle)
{
    const float r = 6.5f;
    g.setColour (juce::Colour (0xff1d1b18));
    g.fillEllipse (c.x - r - 1.5f, c.y - r - 1.5f, (r + 1.5f) * 2, (r + 1.5f) * 2);
    juce::ColourGradient grad (juce::Colour (0xffe2ddd3), c.x - r * 0.4f, c.y - r * 0.5f,
                               juce::Colour (0xff4a453f), c.x + r, c.y + r, true);
    grad.addColour (0.55, juce::Colour (0xff8c857a));
    g.setGradientFill (grad);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    const auto dir = juce::Point<float> (std::cos (angle), std::sin (angle)) * (r - 1.5f);
    g.setColour (juce::Colour (0x59ffffff));
    g.drawLine (c.x - dir.x, c.y - dir.y + 1, c.x + dir.x, c.y + dir.y + 1, 2.0f);
    g.setColour (juce::Colour (0xff3a352f));
    g.drawLine (c.x - dir.x, c.y - dir.y, c.x + dir.x, c.y + dir.y, 2.0f);
}

void drawEngraved (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                   const juce::Font& font, juce::Justification just, juce::Colour colour)
{
    g.setFont (font);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawText (text, area.translated (0, 1), just, false);
    g.setColour (colour);
    g.drawText (text, area, just, false);
}

void drawBezel (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    g.setColour (juce::Colour (0xff9d927e));
    g.fillRoundedRectangle (r.expanded (7), corner + 6);
    g.setColour (col::dark);
    g.fillRoundedRectangle (r.expanded (5), corner + 4);
}
} // namespace bench

//==============================================================================
BenchLookAndFeel::BenchLookAndFeel()
{
    using namespace bench;
    setColour (juce::PopupMenu::backgroundColourId, col::paper);
    setColour (juce::PopupMenu::textColourId, col::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::dark);
    setColour (juce::PopupMenu::highlightedTextColourId, col::lit);
    setColour (juce::ComboBox::textColourId, col::ink);
    setColour (juce::ComboBox::backgroundColourId, col::paper);
    setColour (juce::ComboBox::outlineColourId, col::line);
    setColour (juce::ComboBox::arrowColourId, col::ink);
    setColour (juce::TooltipWindow::backgroundColourId, col::paper);
    setColour (juce::TooltipWindow::textColourId, col::ink);
}

void BenchLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                         float startAngle, float endAngle, juce::Slider&)
{
    using namespace bench;
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float R = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // ticks
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * i / 10.0f;
        const float r0 = R * 0.86f, r1 = (i % 5 == 0) ? R : R * 0.95f;
        const juce::Point<float> d (std::sin (a), -std::cos (a));
        g.setColour (col::ink.withAlpha (0.85f));
        g.drawLine (c.x + d.x * r0, c.y + d.y * r0, c.x + d.x * r1, c.y + d.y * r1, i % 5 == 0 ? 1.6f : 1.2f);
    }

    // skirt with knurling
    const float skirt = R * 0.76f;
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - skirt, c.y - skirt + 2.5f, skirt * 2, skirt * 2);
    g.setColour (juce::Colour (0xff1a1511));
    g.fillEllipse (c.x - skirt, c.y - skirt, skirt * 2, skirt * 2);
    g.setColour (juce::Colour (0xff4d4339));
    for (int i = 0; i < 72; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * i / 72.0f;
        const juce::Point<float> d (std::sin (a), -std::cos (a));
        g.drawLine (c.x + d.x * skirt * 0.9f, c.y + d.y * skirt * 0.9f, c.x + d.x * skirt, c.y + d.y * skirt, 1.0f);
    }

    // bakelite cap
    const float body = R * 0.62f;
    juce::ColourGradient grad (juce::Colour (0xff5a4e44), c.x - body * 0.3f, c.y - body * 0.4f,
                               juce::Colour (0xff0d0a08), c.x + body * 0.9f, c.y + body * 0.9f, true);
    grad.addColour (0.45, juce::Colour (0xff241d18));
    g.setGradientFill (grad);
    g.fillEllipse (c.x - body, c.y - body, body * 2, body * 2);
    g.setColour (juce::Colour (0xff0d0a08));
    g.drawEllipse (c.x - body, c.y - body, body * 2, body * 2, 1.0f);

    // pointer, slightly worn
    const juce::Point<float> d (std::sin (angle), -std::cos (angle));
    g.setColour (juce::Colour (0xffece2cc).withAlpha (0.92f));
    g.drawLine (c.x + d.x * body * 0.18f, c.y + d.y * body * 0.18f, c.x + d.x * skirt * 0.94f, c.y + d.y * skirt * 0.94f, 2.6f);
}

void BenchLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    using namespace bench;
    const float cy = y + height * 0.5f;
    const auto track = juce::Rectangle<float> ((float) x, cy - 3, (float) width, 6);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.fillRoundedRectangle (track.translated (0, 1), 1.5f);
    g.setColour (col::dark);
    g.fillRoundedRectangle (track, 1.5f);

    const auto thumb = juce::Rectangle<float> (sliderPos - 6, cy - 9, 12, 18);
    juce::ColourGradient grad (juce::Colour (0xffd8d2c6), thumb.getX(), 0, juce::Colour (0xffa39b8c), thumb.getRight(), 0, false);
    grad.addColour (0.45, juce::Colours::white);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (thumb, 2);
    g.setColour (juce::Colour (0xff4a4037));
    g.drawRoundedRectangle (thumb, 2, 1);
    g.drawLine (thumb.getCentreX(), thumb.getY() + 4, thumb.getCentreX(), thumb.getBottom() - 4, 1);
}

void BenchLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace bench;
    const auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.isEnabled() ? juce::Colour (0xfff6efe1) : col::well);
    g.fillRoundedRectangle (r, 3);
    g.setColour (col::line);
    g.drawRoundedRectangle (r, 3, 1);
    juce::Path tri;
    const float ax = (float) width - 14, ay = height * 0.5f;
    tri.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (col::ink2);
    g.fillPath (tri);
}

juce::Font BenchLookAndFeel::getComboBoxFont (juce::ComboBox&) { return bench::typewriter (15.0f); }

void BenchLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font BenchLookAndFeel::getPopupMenuFont() { return bench::typewriter (15.0f); }

void BenchLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (bench::col::paper);
    g.setColour (bench::col::line);
    g.drawRect (0, 0, width, height, 1);
}
