#include "Components.h"

using namespace bench;

//==============================================================================
void LampButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const bool on = getToggleState();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    if (on) r = r.translated (0, 1);

    juce::ColourGradient grad (on ? juce::Colour (0xff2f2721) : juce::Colour (0xff4a4037), 0, r.getY(),
                               on ? juce::Colour (0xff3b322a) : juce::Colour (0xff352d26), 0, r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 3);
    if (! on)
    {
        g.setColour (juce::Colours::white.withAlpha (highlighted ? 0.16f : 0.1f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3, r.getRight() - 3);
        g.setColour (juce::Colour (0xff120e0b));
        g.drawHorizontalLine ((int) r.getBottom(), r.getX() + 2, r.getRight() - 2);
    }
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawRoundedRectangle (r.reduced (0.5f), 3, 1.5f);
    }

    const juce::Point<float> lamp (r.getX() + 11, r.getCentreY());
    if (on)
    {
        g.setColour (col::lampOn.withAlpha (0.35f));
        g.fillEllipse (lamp.x - 6, lamp.y - 6, 12, 12);
        g.setColour (col::lampOn);
    }
    else
        g.setColour (col::lampOff);
    g.fillEllipse (lamp.x - 3, lamp.y - 3, 6, 6);

    g.setFont (label (13.5f, 0.16f));
    g.setColour (on ? col::lit : juce::Colour (0xffe9dcc2));
    g.drawText (getButtonText().toUpperCase(), r.withTrimmedLeft (20).withTrimmedRight (6), juce::Justification::centred, false);
}

void MiniButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    if (down) r = r.translated (0, 0.5f);
    g.setColour (highlighted ? juce::Colour (0xfff3ead6) : juce::Colour (0xffe9dec7));
    g.fillRoundedRectangle (r, 2);
    g.setColour (col::line);
    g.drawRoundedRectangle (r, 2, 1);
    g.setFont (label (12.0f, 0.12f));
    g.setColour (col::ink2);
    g.drawText (getButtonText().toUpperCase(), r, juce::Justification::centred, false);
}

//==============================================================================
ChoiceSwitch::ChoiceSwitch (juce::RangedAudioParameter& param, const juce::StringArray& labels, int cols)
    : attachment (param, [this] (float v)
      {
          const int idx = juce::roundToInt (v);
          for (int i = 0; i < buttons.size(); ++i)
              buttons[i]->setToggleState (i == idx, juce::dontSendNotification);
      }),
      columns (cols)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new LampButton (labels[i]));
        b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    attachment.sendInitialUpdate();
}

void ChoiceSwitch::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (col::dark);
    g.fillRoundedRectangle (r, 4);
    g.setColour (juce::Colour (0xff1a1511));
    g.drawRoundedRectangle (r.reduced (0.5f), 4, 1);
}

void ChoiceSwitch::resized()
{
    const auto r = getLocalBounds().reduced (4);
    const int gap = 4, n = buttons.size();
    const int cols = columns > 0 ? columns : n;
    const int rows = (n + cols - 1) / juce::jmax (1, cols);
    const int w = (r.getWidth() - gap * (cols - 1)) / juce::jmax (1, cols);
    const int h = (r.getHeight() - gap * (rows - 1)) / juce::jmax (1, rows);
    for (int i = 0; i < n; ++i)
    {
        const int c = i % cols, row = i / cols;
        const int x = r.getX() + c * (w + gap), y = r.getY() + row * (h + gap);
        const int bw = c == cols - 1 ? r.getRight() - x : w;
        buttons[i]->setBounds (x, y, bw, h - (row == rows - 1 ? 1 : 0));
    }
}

//==============================================================================
ToggleSwitch::ToggleSwitch (juce::RangedAudioParameter& param, const juce::StringArray& l)
    : labels (l),
      attachment (param, [this] (float v)
      {
          current = juce::jlimit (0, labels.size() - 1, juce::roundToInt (v));
          repaint();
      })
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle (param.getName (64));
    attachment.sendInitialUpdate();
}

juce::Rectangle<float> ToggleSwitch::leverArea() const
{
    return getLocalBounds().toFloat().removeFromLeft (38.0f);
}

int ToggleSwitch::positionAt (float y) const
{
    const int n = labels.size();
    return juce::jlimit (0, n - 1, (int) (y / (float) getHeight() * (float) n));
}

void ToggleSwitch::select (int index)
{
    if (index != current)
        attachment.setValueAsCompleteGesture ((float) index);
}

void ToggleSwitch::mouseDown (const juce::MouseEvent& e) { select (positionAt ((float) e.y)); }
void ToggleSwitch::mouseDrag (const juce::MouseEvent& e) { select (positionAt ((float) e.y)); }

void ToggleSwitch::paint (juce::Graphics& g)
{
    const int n = labels.size();
    const auto lever = leverArea();
    const float rowH = (float) getHeight() / (float) n;
    const juce::Point<float> c (lever.getCentreX(), (float) getHeight() * 0.5f);

    // engraved position marks
    for (int i = 0; i < n; ++i)
    {
        const float y = rowH * ((float) i + 0.5f);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawLine (lever.getRight() - 6, y + 1, lever.getRight() + 2, y + 1, 1.0f);
        g.setColour (col::engrave);
        g.drawLine (lever.getRight() - 6, y, lever.getRight() + 2, y, 1.0f);
    }

    // hex nut
    juce::Path hex;
    const float hr = 14.0f;
    for (int k = 0; k < 6; ++k)
    {
        const float a = juce::MathConstants<float>::pi / 3.0f * (float) k + juce::MathConstants<float>::pi / 6.0f;
        const juce::Point<float> pt (c.x + std::cos (a) * hr, c.y + std::sin (a) * hr);
        if (k == 0) hex.startNewSubPath (pt); else hex.lineTo (pt);
    }
    hex.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillPath (hex, juce::AffineTransform::translation (0, 2));
    juce::ColourGradient nut (juce::Colour (0xffe9e4da), c.x - hr, c.y - hr, juce::Colour (0xff7a7368), c.x + hr, c.y + hr, false);
    nut.addColour (0.5, juce::Colour (0xffb8b1a4));
    g.setGradientFill (nut);
    g.fillPath (hex);
    g.setColour (juce::Colour (0xff4a453f));
    g.strokePath (hex, juce::PathStrokeType (1.0f));
    g.setColour (juce::Colour (0xff2a2622));
    g.fillEllipse (c.x - 7.5f, c.y - 7.5f, 15, 15);

    // lever: up, centre (pointing at you) or down
    const float t = n == 2 ? (current == 0 ? -1.0f : 1.0f) : (float) (current - 1);
    const float reach = juce::jmin (26.0f, (float) getHeight() * 0.5f - 7.0f);
    const juce::Point<float> tip (c.x, c.y + t * reach);
    if (std::abs (t) > 0.01f)
    {
        juce::Path bat;
        const float w0 = 3.2f, w1 = 4.6f;
        bat.startNewSubPath (c.x - w0, c.y);
        bat.lineTo (tip.x - w1, tip.y);
        bat.lineTo (tip.x + w1, tip.y);
        bat.lineTo (c.x + w0, c.y);
        bat.closeSubPath();
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillPath (bat, juce::AffineTransform::translation (2, 2));
        juce::ColourGradient chrome (juce::Colour (0xff8d877c), c.x - w1, 0, juce::Colour (0xff6c665c), c.x + w1, 0, false);
        chrome.addColour (0.35, juce::Colours::white);
        g.setGradientFill (chrome);
        g.fillPath (bat);
    }
    const float ball = std::abs (t) > 0.01f ? 5.6f : 7.0f;
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillEllipse (tip.x - ball + 1.5f, tip.y - ball + 2.0f, ball * 2, ball * 2);
    juce::ColourGradient ballGrad (juce::Colours::white, tip.x - ball * 0.4f, tip.y - ball * 0.5f,
                                   juce::Colour (0xff6c665c), tip.x + ball, tip.y + ball, true);
    g.setGradientFill (ballGrad);
    g.fillEllipse (tip.x - ball, tip.y - ball, ball * 2, ball * 2);

    // labels
    for (int i = 0; i < n; ++i)
    {
        auto row = juce::Rectangle<float> (lever.getRight() + 8, rowH * (float) i, (float) getWidth() - lever.getRight() - 8, rowH);
        const bool on = i == current;
        if (lampFor && lampFor (i))
        {
            const auto lamp = row.removeFromRight (12).withSizeKeepingCentre (7, 7);
            g.setColour (col::lampOn.withAlpha (0.35f));
            g.fillEllipse (lamp.expanded (3));
            g.setColour (col::lampOn);
            g.fillEllipse (lamp);
        }
        if (on) drawEngraved (g, labels[i].toUpperCase(), row, bench::label (14.5f, 0.14f), juce::Justification::centredLeft, col::ink);
        else    drawEngraved (g, labels[i].toUpperCase(), row, bench::label (14.5f, 0.14f), juce::Justification::centredLeft, col::ink3.withAlpha (0.8f));
    }
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
            const juce::String& labelText, const juce::String& hintText, int size)
    : param (state.getParameter (paramId)), label (labelText), hint (hintText), dialSize (size)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
    slider.setTitle (labelText);
    addAndMakeVisible (slider);
}

void Knob::resized()
{
    const int size = juce::jmin (getWidth(), dialSize);
    slider.setBounds ((getWidth() - size) / 2, 0, size, size);
}

void Knob::paint (juce::Graphics& g)
{
    const bool small = dialSize < 80;
    auto r = getLocalBounds().toFloat().withTrimmedTop ((float) slider.getBottom() + 2);
    const auto labelRow = r.removeFromTop (small ? 18.0f : 22.0f);
    const auto labelFont = bench::label (small ? 14.0f : 17.0f, 0.2f);
    drawEngraved (g, label.toUpperCase(), labelRow, labelFont, juce::Justification::centred, col::ink);

    if (meter)
    {
        const float w = juce::GlyphArrangement::getStringWidth (labelFont, label.toUpperCase());
        const auto lamp = juce::Rectangle<float> (labelRow.getCentreX() + w * 0.5f + 8, labelRow.getCentreY() - 4, 8, 8);
        const float v = juce::jlimit (0.0f, 1.0f, lastMeter);
        if (v > 0.02f)
        {
            g.setColour (col::lampOn.withAlpha (0.4f * v));
            g.fillEllipse (lamp.expanded (4 * v));
        }
        g.setColour (col::lampOff.interpolatedWith (col::lampOn, v));
        g.fillEllipse (lamp);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawEllipse (lamp, 0.8f);
    }

    g.setFont (typewriter (small ? 14.0f : 15.0f));
    g.setColour (col::accent);
    g.drawText (param->getCurrentValueAsText(), r.removeFromTop (18), juce::Justification::centred, false);
    if (hint.isNotEmpty())
        drawEngraved (g, hint, r.removeFromTop (16), bench::label (12.5f, 0.06f), juce::Justification::centred);
}

void Knob::paintOverChildren (juce::Graphics& g)
{
    if (lastMod.depth <= 0.0005f) return;
    const auto b = slider.getBounds().toFloat();
    const auto c = b.getCentre();
    const float R = (juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 2.0f) * 0.81f;
    const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
    const float v = (float) param->getValue();
    const float lo = juce::jlimit (0.0f, 1.0f, v - 0.5f * lastMod.depth);
    const float hi = juce::jlimit (0.0f, 1.0f, v + 0.5f * lastMod.depth);
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, R, R, 0, a0 + lo * (a1 - a0), a0 + hi * (a1 - a0), true);
    g.setColour (col::accent.withAlpha (0.85f));
    g.strokePath (arc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float now = juce::jlimit (0.0f, 1.0f, v + 0.5f * lastMod.depth * lastMod.lfo);
    const float a = a0 + now * (a1 - a0);
    const juce::Point<float> dot (c.x + std::sin (a) * R, c.y - std::cos (a) * R);
    g.setColour (col::lit);
    g.fillEllipse (dot.x - 3.2f, dot.y - 3.2f, 6.4f, 6.4f);
    g.setColour (col::accent);
    g.drawEllipse (dot.x - 3.2f, dot.y - 3.2f, 6.4f, 6.4f, 1.2f);
}

void Knob::refreshIndicators()
{
    if (modulation)
    {
        const auto m = modulation();
        if (std::abs (m.depth - lastMod.depth) > 0.001f || (m.depth > 0.0005f && std::abs (m.lfo - lastMod.lfo) > 0.01f))
        {
            lastMod = m;
            repaint();
        }
    }
    if (meter)
    {
        const float v = meter();
        if (std::abs (v - lastMeter) > 0.02f) { lastMeter = v; repaint(); }
    }
}

//==============================================================================
void drawDymo (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float rotation)
{
    const auto font = tape (12.5f);
    const auto t = text.toUpperCase();
    const float w = juce::jmin (area.getWidth(), juce::GlyphArrangement::getStringWidth (font, t) + 18.0f);
    const auto tapeRect = juce::Rectangle<float> (area.getX(), area.getCentreY() - 9.5f, w, 19.0f);

    juce::Graphics::ScopedSaveState s (g);
    g.addTransform (juce::AffineTransform::rotation (rotation, tapeRect.getCentreX(), tapeRect.getCentreY()));
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (tapeRect.translated (0, 1.5f), 2);
    juce::ColourGradient grad (juce::Colour (0xff262626), 0, tapeRect.getY(), juce::Colour (0xff111111), 0, tapeRect.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (tapeRect, 2);
    g.setFont (font);
    const auto textArea = tapeRect.reduced (9, 0);
    g.setColour (juce::Colours::black);
    g.drawText (t, textArea.translated (0, -1), juce::Justification::centredLeft, true);
    g.setColour (juce::Colour (0xffecebe6));
    g.drawText (t, textArea, juce::Justification::centredLeft, true);
}

void drawWaveform (juce::Graphics& g, const std::vector<float>& peaks, juce::Rectangle<float> area, juce::Colour colour)
{
    g.setColour (col::ink.withAlpha (0.12f));
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
    const int cols = (int) peaks.size() / 2;
    if (cols < 2) return;

    float mx = 0.001f;
    for (float v : peaks) mx = juce::jmax (mx, std::abs (v));

    const int n = juce::jmax (2, (int) (area.getWidth() / 2.0f));
    std::vector<float> up ((size_t) n), dn ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const int c0 = i * cols / n, c1 = juce::jmax (c0 + 1, (i + 1) * cols / n);
        float a = 0, b = 0;
        for (int c = c0; c < c1 && c < cols; ++c)
        {
            a = juce::jmin (a, peaks[(size_t) c * 2]);
            b = juce::jmax (b, peaks[(size_t) c * 2 + 1]);
        }
        up[(size_t) i] = b / mx;
        dn[(size_t) i] = a / mx;
    }
    auto smooth = [n] (std::vector<float>& v)
    {
        std::vector<float> o (v.size());
        for (int i = 0; i < n; ++i)
            o[(size_t) i] = (v[(size_t) juce::jmax (0, i - 1)] + 2 * v[(size_t) i] + v[(size_t) juce::jmin (n - 1, i + 1)]) * 0.25f;
        v.swap (o);
    };
    smooth (up);
    smooth (dn);

    const float mid = area.getCentreY(), amp = area.getHeight() * 0.46f;
    juce::Path p;
    p.startNewSubPath (area.getX(), mid - up[0] * amp);
    for (int i = 1; i < n; ++i)
        p.lineTo (area.getX() + area.getWidth() * i / (n - 1), mid - up[(size_t) i] * amp);
    for (int i = n - 1; i >= 0; --i)
        p.lineTo (area.getX() + area.getWidth() * i / (n - 1), mid - juce::jmin (dn[(size_t) i] * amp, -0.5f));
    p.closeSubPath();
    g.setColour (colour.withAlpha (0.82f));
    g.fillPath (p);
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (0.8f));
}

//==============================================================================
static bool isAudioFile (const juce::String& path)
{
    return path.matchesWildcard ("*.wav", true) || path.matchesWildcard ("*.aif", true) || path.matchesWildcard ("*.aiff", true)
        || path.matchesWildcard ("*.mp3", true) || path.matchesWildcard ("*.flac", true) || path.matchesWildcard ("*.ogg", true)
        || path.matchesWildcard ("*.m4a", true);
}

SlotComponent::SlotComponent (BodenlegerProcessor& p, int i) : proc (p), index (i)
{
    addAndMakeVisible (loadButton);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (level);
    addAndMakeVisible (air);
    levelAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::slotLevel (index), level);
    airAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::slotAir (index), air);
    level.onValueChange = [this] { repaint(); };

    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a sound into slot " + juce::String (index + 1),
                                                       juce::File(), "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg;*.m4a");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile()) load (f);
                              });
    };
    clearButton.onClick = [this] { proc.clearSample (index); refresh(); };
    refresh();
}

void SlotComponent::load (const juce::File& f)
{
    error.clear();
    if (! proc.loadSample (index, f))
        error = "Couldn't read that file. Try WAV, AIFF or MP3.";
    shown = nullptr;
    refresh();
    repaint();
}

void SlotComponent::refresh()
{
    auto s = proc.getSample (index);
    if (s != shown)
    {
        shown = s;
        if (shown != nullptr) error.clear();
        repaint();
    }
    clearButton.setVisible (shown != nullptr);
}

bool SlotComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (isAudioFile (f)) return true;
    return false;
}

void SlotComponent::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files)
        if (isAudioFile (f)) { load (juce::File (f)); break; }
    repaint();
}

void SlotComponent::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto head = r.removeFromTop (22);
    loadButton.setBounds (head.removeFromRight (44).reduced (0, 1));
    head.removeFromRight (4);
    clearButton.setBounds (head.removeFromRight (48).reduced (0, 1));
    r.removeFromTop (30); // waveform
    auto row = r.removeFromTop (18);
    row.removeFromLeft (40);
    level.setBounds (row.removeFromLeft (row.getWidth() / 2 - 34));
    row.removeFromLeft (38);
    row.removeFromRight (26);
    air.setBounds (row);
}

void SlotComponent::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.fillRoundedRectangle (r.translated (0, 1), 2);
    g.setColour (col::paper);
    g.fillRoundedRectangle (r, 2);
    g.setColour (dragOver ? col::accent : juce::Colour (0xffc4b493));
    g.drawRoundedRectangle (r, 2, dragOver ? 2.0f : 1.0f);

    auto inner = getLocalBounds().toFloat().reduced (8);
    auto head = inner.removeFromTop (22);
    g.setFont (typewriter (14.0f));
    g.setColour (col::ink3);
    g.drawText (juce::String (index + 1), head.removeFromLeft (16), juce::Justification::centredLeft, false);
    head.removeFromRight ((float) (getWidth() - clearButton.getX()) - 4.0f);

    if (shown != nullptr)
        drawDymo (g, shown->name, head.reduced (2, 0), (index % 2 == 0 ? -0.012f : 0.009f));
    else
    {
        g.setFont (typewriter (14.0f));
        g.setColour (error.isNotEmpty() ? col::accent : col::ink3);
        g.drawText (error.isNotEmpty() ? error : juce::String ("Drop audio here"), head, juce::Justification::centredLeft, true);
    }

    inner.removeFromTop (4);
    const auto wave = inner.removeFromTop (24);
    if (shown != nullptr)
        drawWaveform (g, shown->peaks, wave, juce::Colour (0xff3a2a1c));
    else
    {
        g.setColour (col::ink.withAlpha (0.12f));
        g.drawHorizontalLine ((int) wave.getCentreY(), wave.getX(), wave.getRight());
    }

    inner.removeFromTop (2);
    auto row = inner.removeFromTop (18);
    const auto f = bench::label (11.5f, 0.16f);
    drawEngraved (g, "LEVEL", row.removeFromLeft (40), f, juce::Justification::centredLeft);
    drawEngraved (g, "TONE", juce::Rectangle<float> ((float) level.getRight() + 4, row.getY(), 34, row.getHeight()), f, juce::Justification::centredLeft);
    drawEngraved (g, "AIR", juce::Rectangle<float> ((float) air.getRight() + 4, row.getY(), 26, row.getHeight()), f, juce::Justification::centredLeft);
}

//==============================================================================
ReturnPanel::ReturnPanel (BodenlegerProcessor& p) : proc (p)
{
    addAndMakeVisible (air);
    airAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::returnAir, air);
    follow.setClickingTogglesState (true);
    followAtt = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter (ids::follow), follow);
    addAndMakeVisible (follow);
}

void ReturnPanel::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (44); // note
    auto card = r.removeFromTop (150);
    auto inner = card.reduced (8);
    inner.removeFromTop (26);
    scopeArea = inner.removeFromTop (88).toFloat();
    inner.removeFromTop (6);
    auto row = inner.removeFromTop (18);
    row.removeFromLeft (40);
    row.removeFromRight (30);
    air.setBounds (row);
    r.removeFromTop (12);
    follow.setBounds (r.removeFromTop (28).removeFromLeft (170));
}

void ReturnPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setFont (typewriter (13.5f));
    g.setColour (col::ink2);
    g.drawFittedText ("Put Bodenleger on a return and send it whatever should become the pad. It listens to the last 7.5 seconds.",
                      r.removeFromTop (44).toNearestInt(), juce::Justification::topLeft, 3);

    const auto card = r.removeFromTop (150).reduced (0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.fillRoundedRectangle (card.translated (0, 1), 2);
    g.setColour (col::paper);
    g.fillRoundedRectangle (card, 2);
    g.setColour (juce::Colour (0xffc4b493));
    g.drawRoundedRectangle (card, 2, 1);
    drawDymo (g, "Return input", card.reduced (8).removeFromTop (22), -0.01f);

    // input scope: last 10 s, shaded listening window
    const double sr = proc.engine.getSampleRate();
    const int total = (int) (10.0 * sr);
    scratch.resize ((size_t) total);
    const auto written = proc.engine.copyRecentInput (scratch.data(), total);
    const auto a = scopeArea;
    g.setColour (col::accent.withAlpha (0.13f));
    g.fillRect (a.withTrimmedLeft (a.getWidth() * 0.25f));
    if (written > 0)
    {
        const int cols = (int) a.getWidth();
        std::vector<float> peaks ((size_t) cols * 2);
        for (int c = 0; c < cols; ++c)
        {
            float lo = 0, hi = 0;
            const int s0 = c * total / cols, s1 = (c + 1) * total / cols;
            for (int s = s0; s < s1; s += 8) { lo = juce::jmin (lo, scratch[(size_t) s]); hi = juce::jmax (hi, scratch[(size_t) s]); }
            peaks[(size_t) c * 2] = lo;
            peaks[(size_t) c * 2 + 1] = hi;
        }
        drawWaveform (g, peaks, a, juce::Colour (0xff3a2a1c));
    }
    else
    {
        g.setFont (typewriter (14.0f));
        g.setColour (col::ink3);
        g.drawText ("Waiting for audio on the return", a, juce::Justification::centred, false);
    }
    g.setColour (col::accent);
    g.fillRect (a.getRight() - 2, a.getY(), 2.0f, a.getHeight());

    const auto f = bench::label (11.5f, 0.16f);
    drawEngraved (g, "TONE", juce::Rectangle<float> (card.getX() + 8, (float) air.getY(), 40, (float) air.getHeight()), f, juce::Justification::centredLeft);
    drawEngraved (g, "AIR", juce::Rectangle<float> ((float) air.getRight() + 4, (float) air.getY(), 30, (float) air.getHeight()), f, juce::Justification::centredLeft);

    auto bottom = getLocalBounds().toFloat().withTop ((float) follow.getBottom() + 10);
    g.setFont (typewriter (13.0f));
    g.setColour (col::ink3);
    g.drawFittedText ("Follow recaptures every 8 seconds and crossfades. Turn it off to freeze the pad, then use Capture now.",
                      bottom.toNearestInt(), juce::Justification::topLeft, 3);
}

//==============================================================================
SpectrumView::SpectrumView (BodenlegerProcessor& p) : proc (p)
{
    fftData.assign (8192, 0.0f);
    liveDb.assign (2048, -120.0f);
    window.resize (4096);
    for (int i = 0; i < 4096; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / 4096.0f);
    setOpaque (true);
}

void SpectrumView::updateLive()
{
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    proc.engine.copyRecentOutput (fftData.data(), 4096);
    for (int i = 0; i < 4096; ++i) fftData[(size_t) i] *= window[(size_t) i];
    fft.performFrequencyOnlyForwardTransform (fftData.data());
    for (int k = 0; k < 2048; ++k)
    {
        const float mag = fftData[(size_t) k] * 4.0f / 4096.0f;
        const float prevLin = std::pow (10.0f, liveDb[(size_t) k] / 20.0f);
        const float lin = prevLin * 0.85f + mag * 0.15f;
        liveDb[(size_t) k] = 20.0f * std::log10 (lin + 1e-9f);
    }
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float W = r.getWidth(), H = r.getHeight();
    {
        juce::ColourGradient bg (juce::Colour (0xfff7dc9e), 0, 0, juce::Colour (0xffe5ae5a), 0, H, false);
        g.setGradientFill (bg);
        g.fillRect (r);
        juce::ColourGradient glow (juce::Colour (0x59fff0c8), W * 0.5f, H * 0.55f, juce::Colour (0x405a280a), W * 0.5f + W * 0.7f, H * 0.55f, true);
        g.setGradientFill (glow);
        g.fillRect (r);
    }

    const float fmin = 40.0f, fmax = 14000.0f, top = 10.0f, bot = H - 20.0f, h = bot - top;
    auto xOf = [&] (float f) { return std::log (f / fmin) / std::log (fmax / fmin) * W; };

    const auto key = proc.getEffectiveKey();
    const auto notes = bl::maskNotes (key, proc.getKeyMode());
    g.setFont (typewriter (12.0f));
    for (int m = 28; m <= 118; ++m)
    {
        const float f = 440.0f * std::pow (2.0f, (m - 69) / 12.0f);
        if (f < fmin || f > fmax) continue;
        const int pc = m % 12;
        const float x = xOf (f);
        const bool inScale = std::find (notes.begin(), notes.end(), pc) != notes.end();
        if (pc == key.root) { g.setColour (juce::Colour (0x61461e0a)); g.fillRect (x, top, 1.0f, h); }
        else if (inScale) { g.setColour (juce::Colour (0x1f461e0a)); g.fillRect (x, top, 1.0f, h); }
        if (pc == 0)
        {
            g.setColour (juce::Colour (0xa63c1e0a));
            g.drawText ("C" + juce::String (m / 12 - 1), juce::Rectangle<float> (x + 3, H - 18, 30, 16), juce::Justification::centredLeft, false);
        }
    }

    const auto d = proc.getDisplay();
    if (d == nullptr || d->mask.size() < (size_t) bl::kBins)
    {
        g.setFont (typewriter (16.0f));
        g.setColour (juce::Colour (0xff5a3a1a));
        g.drawText (proc.getSourceMode() == 0 ? "Waiting for audio on the return" : "Load a sound into a slot to build a pad",
                    r, juce::Justification::centred, false);
    }
    else
    {
        const int cols = juce::jmax (2, (int) W);
        std::vector<float> raw ((size_t) cols), ton ((size_t) cols), noi ((size_t) cols);
        float w[bl::kSnapshots];
        for (int k = 0; k < bl::kSnapshots; ++k) w[k] = proc.engine.snapshotWeight[k].load();
        const double binHz = d->sampleRate / bl::kAnalysisSize;
        float ref = 1e-9f, nref = 1e-9f;
        for (int x = 0; x < cols; ++x)
        {
            const double f0 = fmin * std::pow (fmax / fmin, (double) x / cols), f1 = fmin * std::pow (fmax / fmin, (double) (x + 1) / cols);
            const int k0 = juce::jmax (1, (int) (f0 / binHz)), k1 = juce::jmax (k0 + 1, (int) std::ceil (f1 / binHz));
            float a = 0, t = 0, n = 0;
            for (int k = k0; k < k1 && k < bl::kBins; ++k)
            {
                float c = 0, z = 0;
                for (int s = 0; s < bl::kSnapshots; ++s)
                {
                    c += w[s] * d->snapshots[(size_t) s].tonal[(size_t) k];
                    z += w[s] * d->snapshots[(size_t) s].noise[(size_t) k];
                }
                a = juce::jmax (a, c);
                t = juce::jmax (t, c * d->mask[(size_t) k]);
                n = juce::jmax (n, z);
            }
            raw[(size_t) x] = a; ton[(size_t) x] = t; noi[(size_t) x] = n;
        }
        auto smooth = [cols] (std::vector<float>& v)
        {
            for (int pass = 0; pass < 2; ++pass)
            {
                float prev = v[0];
                for (int x = 1; x < cols - 1; ++x) { const float c = v[(size_t) x]; v[(size_t) x] = (prev + 2 * c + v[(size_t) x + 1]) * 0.25f; prev = c; }
            }
        };
        smooth (raw); smooth (ton); smooth (noi);
        for (int x = 0; x < cols; ++x) { ref = juce::jmax (ref, raw[(size_t) x]); nref = juce::jmax (nref, noi[(size_t) x]); }

        const float tonal = proc.apvts.getRawParameterValue (ids::tonal)->load();
        const float nz = proc.apvts.getRawParameterValue (ids::noise)->load();
        auto yOf = [&] (float v, float rf) { const float db = 20.0f * std::log10 (v / rf + 1e-9f); return bot - juce::jmax (0.0f, (db + 60.0f) / 60.0f) * h; };
        auto area = [&] (const std::vector<float>& arr, float rf, float scale)
        {
            juce::Path p;
            p.startNewSubPath (0, bot);
            for (int x = 0; x < cols; ++x) p.lineTo ((float) x * W / (cols - 1), bot - (bot - yOf (arr[(size_t) x], rf)) * scale);
            p.lineTo (W, bot);
            p.closeSubPath();
            return p;
        };

        g.setColour (juce::Colour::fromRGB (90, 95, 80).withAlpha (0.1f + 0.3f * nz));
        g.fillPath (area (noi, nref, 0.35f + 0.6f * nz));

        auto rawPath = area (raw, ref, 1.0f);
        g.setColour (juce::Colour::fromRGB (150, 55, 25).withAlpha (0.06f + 0.28f * (1.0f - tonal)));
        g.fillPath (rawPath);
        g.setColour (juce::Colour::fromRGB (150, 55, 25).withAlpha (0.35f + 0.4f * (1.0f - tonal)));
        g.strokePath (rawPath, juce::PathStrokeType (1.4f));

        auto tonPath = area (ton, ref, 1.0f);
        g.setColour (juce::Colour::fromRGB (50, 26, 12).withAlpha (0.05f + 0.35f * tonal));
        g.fillPath (tonPath);
        g.setColour (juce::Colour::fromRGB (40, 18, 8).withAlpha (0.25f + 0.7f * tonal));
        g.strokePath (tonPath, juce::PathStrokeType (1.4f));
    }

    if (proc.isAudioRunning())
    {
        const double sr = proc.engine.getSampleRate();
        juce::Path live;
        bool started = false;
        for (int x = 0; x < (int) W; x += 2)
        {
            const double f = fmin * std::pow (fmax / fmin, (double) x / W);
            const int k = juce::jlimit (0, 2047, (int) std::lround (f / (sr / 4096.0)));
            const float y = bot - juce::jlimit (0.0f, 1.0f, (liveDb[(size_t) k] + 100.0f) / 80.0f) * h;
            if (! started) { live.startNewSubPath ((float) x, y); started = true; }
            else live.lineTo ((float) x, y);
        }
        g.setColour (juce::Colour::fromRGB (120, 20, 5).withAlpha (0.8f));
        g.strokePath (live, juce::PathStrokeType (1.2f));
    }

    // glass edge
    for (int i = 0; i < 6; ++i)
    {
        g.setColour (juce::Colour::fromRGB (60, 30, 8).withAlpha (0.07f * (6 - i) / 6.0f));
        g.drawRect (r.reduced ((float) i), 1.0f);
    }
}

//==============================================================================
void VUMeter::setLevel (float rms)
{
    const float target = juce::jlimit (-20.0f, 3.0f, 20.0f * std::log10 (rms + 1e-9f) + 18.0f);
    const float next = vu + (target - vu) * 0.12f;
    if (std::abs (next - vu) > 0.01f) { vu = next; repaint(); }
}

void VUMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff9d927e));
    g.fillRoundedRectangle (r, 7);
    r = r.reduced (2);
    g.setColour (col::dark);
    g.fillRoundedRectangle (r, 6);
    r = r.reduced (3);
    juce::ColourGradient face (juce::Colour (0xfffbe9bd), 0, r.getY(), juce::Colour (0xffefcf86), 0, r.getBottom(), false);
    g.setGradientFill (face);
    g.fillRoundedRectangle (r, 3);

    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (r.toNearestInt());
    const float cx = r.getCentreX(), cy = r.getY() + r.getHeight() * 1.25f, R = r.getHeight() * 1.02f;
    auto ang = [] (float v) { return juce::degreesToRadians (-48.0f + (v + 20.0f) / 23.0f * 96.0f); };
    auto pt = [&] (float a, float rad) { return juce::Point<float> (cx + std::sin (a) * rad, cy - std::cos (a) * rad); };

    juce::Path arc;
    arc.addCentredArc (cx, cy, R, R, 0, ang (-20), ang (0), true);
    g.setColour (col::ink);
    g.strokePath (arc, juce::PathStrokeType (1.2f));
    juce::Path red;
    red.addCentredArc (cx, cy, R, R, 0, ang (0), ang (3), true);
    g.setColour (col::accent);
    g.strokePath (red, juce::PathStrokeType (3.0f));

    g.setFont (bench::label (10.0f, 0.0f));
    for (float v : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
    {
        const float a = ang (v);
        g.setColour (v >= 0 ? col::accent : col::ink);
        g.drawLine (juce::Line<float> (pt (a, R), pt (a, R + 5)), 1.2f);
        if (v == -20 || v == -10 || v == -5 || v == 0 || v == 3)
        {
            const auto p = pt (a, R + 12);
            g.drawText (v > 0 ? "+" + juce::String ((int) v) : juce::String ((int) v),
                        juce::Rectangle<float> (p.x - 12, p.y - 6, 24, 12), juce::Justification::centred, false);
        }
    }
    g.setColour (col::ink);
    g.setFont (bench::label (12.0f, 0.1f));
    g.drawText ("VU", juce::Rectangle<float> (cx - 20, r.getY() + r.getHeight() * 0.58f, 40, 14), juce::Justification::centred, false);

    g.setColour (juce::Colour (0xff1a120c));
    g.drawLine (juce::Line<float> ({ cx, cy }, pt (ang (vu), R + 6)), 1.4f);

    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.35f), 0, r.getY(), juce::Colours::white.withAlpha (0.0f), 0, r.getCentreY(), false);
    g.setGradientFill (sheen);
    g.fillRect (r.withHeight (r.getHeight() * 0.5f));
}

//==============================================================================
void KeyButton::setState (bool inScale, bool isRoot)
{
    if (inScale != in || isRoot != root) { in = inScale; root = isRoot; repaint(); }
}

void KeyButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (root ? col::dark : in ? juce::Colour (0xfffffaf0) : juce::Colour (0xfff6efe1));
    g.fillRoundedRectangle (r, 2);
    g.setColour (root ? col::dark : highlighted ? col::ink3 : col::line);
    g.drawRoundedRectangle (r, 2, 1);
    g.setFont (typewriter (15.0f));
    g.setColour (root ? col::lit : in ? col::ink : col::ink3);
    g.drawText (getButtonText(), r, juce::Justification::centred, false);
}
