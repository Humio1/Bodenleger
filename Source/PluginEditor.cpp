#include "PluginEditor.h"

using namespace bench;

namespace
{
constexpr int kW = MainPanel::kW, kH = MainPanel::kH, kEar = 34;
constexpr int kHeaderH = 100, kBodyBottom = 530, kRackRight = 354, kKnobBottom = 690, kCharRight = 532;
const char* kNoteNames[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
const char* kScaleNames[] = { "minor", "major", "root + 5th" };
const char* kKeyModes[] = { "scale", "chord", "shimmer" };
const char* kNoiseModes[] = { "air", "dust", "wind" };
} // namespace

MainPanel::MainPanel (BodenlegerProcessor& p)
    : proc (p),
      sourceSwitch (*p.apvts.getParameter (ids::source), { "Return", "Samples" }),
      modeSwitch (*p.apvts.getParameter (ids::mode), { "Drone", "Wash" }),
      returnPanel (p), spectrum (p),
      keyModeSwitch (*p.apvts.getParameter (ids::keyMode), { "Scale", "Chord", "Shimmer" }),
      noiseModeSwitch (*p.apvts.getParameter (ids::noiseMode), { "Air", "Dust", "Wind" }),
      breathModeSwitch (*p.apvts.getParameter (ids::breathMode), { "Smooth", "Pump", "Gate" }),
      listenSwitch (*p.apvts.getParameter (ids::breathListen), { "Input", "Sidechain" }),
      lfoShapes (*p.apvts.getParameter (ids::lfoShape), { "Sine", "Tri", "Square", "Random" }, 2),
      rateKnob (p.apvts, ids::lfoRate, "Rate", {}, 62),
      divisionKnob (p.apvts, ids::lfoDivision, "Rate", {}, 62),
      depthKnob (p.apvts, ids::lfoDepth, "Depth", {}, 62)
{
    addAndMakeVisible (sourceSwitch);
    addAndMakeVisible (modeSwitch);
    addAndMakeVisible (vu);
    addAndMakeVisible (spectrum);
    addAndMakeVisible (capture);
    addChildComponent (returnPanel);

    for (int i = 0; i < BodenlegerProcessor::kSlots; ++i)
    {
        slots[(size_t) i] = std::make_unique<SlotComponent> (p, i);
        addChildComponent (*slots[(size_t) i]);
    }

    capture.onClick = [this] { proc.requestBuild (true); };

    autoKey.setClickingTogglesState (true);
    autoAtt = std::make_unique<juce::ButtonParameterAttachment> (*p.apvts.getParameter (ids::keyAuto), autoKey);
    autoKey.onClick = [this]
    {
        if (! autoKey.getToggleState())
        {
            const auto k = proc.getDetectedKey();
            setParam (ids::keyRoot, (float) k.root);
            setParam (ids::keyScale, (float) k.scale);
        }
    };
    addAndMakeVisible (autoKey);

    scaleBox.addItem ("Minor", 1);
    scaleBox.addItem ("Major", 2);
    scaleBox.addItem ("Root + 5th", 3);
    scaleBox.onChange = [this] { chooseKey (proc.getEffectiveKey().root, scaleBox.getSelectedId() - 1); };
    addAndMakeVisible (scaleBox);

    for (int i = 0; i < 12; ++i)
    {
        auto* b = keys.add (new KeyButton (kNoteNames[i]));
        b->onClick = [this, i] { chooseKey (i, proc.getEffectiveKey().scale); };
        b->setTooltip ("Set the root to " + juce::String (kNoteNames[i]));
        addAndMakeVisible (b);
    }

    auto* tonal  = knobs.add (new Knob (p.apvts, ids::tonal,  "Tonal",  "Noise <> in key"));
    auto* noise  = knobs.add (new Knob (p.apvts, ids::noise,  "Noise",  "Noise layer"));
    knobs.add (new Knob (p.apvts, ids::drift,  "Drift",  "Slow movement"));
    auto* color  = knobs.add (new Knob (p.apvts, ids::color,  "Color",  "Dark <> bright"));
    auto* breath = knobs.add (new Knob (p.apvts, ids::breath, "Breath", "Sidechain depth"));
    knobs.add (new Knob (p.apvts, ids::mix,    "Mix",    "Dry <> pad"));
    for (auto* k : knobs) addAndMakeVisible (k);
    tonal->modulation = [this] { return modFor (ids::modTonal); };
    noise->modulation = [this] { return modFor (ids::modNoise); };
    color->modulation = [this] { return modFor (ids::modColor); };
    breath->meter = [this] { return proc.engine.duckAmount.load(); };

    // character
    for (auto* t : { &keyModeSwitch, &noiseModeSwitch, &breathModeSwitch, &listenSwitch })
        addAndMakeVisible (t);
    listenSwitch.lampFor = [this] (int pos) { return pos == 1 && proc.isSidechainActive(); };
    keyModeSwitch.setTooltip ("How the pad is pulled into key: all scale notes, only chord tones, or scale plus an octave-up shimmer");
    noiseModeSwitch.setTooltip ("Character of the noise layer: smooth air, vinyl-like dust, or sweeping wind");
    breathModeSwitch.setTooltip ("How Breath reacts: Smooth follows the energy, Pump hits hard and lets go fast, Gate chops");
    listenSwitch.setTooltip ("What Breath listens to: the plugin's input, or the sidechain input (route a kick to it in your DAW)");

    // modulation
    addAndMakeVisible (lfoShapes);
    lfoSync.setClickingTogglesState (true);
    syncAtt = std::make_unique<juce::ButtonParameterAttachment> (*p.apvts.getParameter (ids::lfoSync), lfoSync);
    addAndMakeVisible (lfoSync);
    addAndMakeVisible (rateKnob);
    addChildComponent (divisionKnob);
    addAndMakeVisible (depthKnob);
    const std::pair<const char*, const char*> targetDefs[] = { { ids::modColor, "Color" }, { ids::modTonal, "Tonal" },
                                                                { ids::modNoise, "Noise" }, { ids::modLevel, "Level" },
                                                                { ids::modPan, "Pan" } };
    for (auto& [id, name] : targetDefs)
    {
        auto* b = targets.add (new LampButton (name));
        b->setClickingTogglesState (true);
        targetAtts.add (new juce::ButtonParameterAttachment (*p.apvts.getParameter (id), *b));
        addAndMakeVisible (b);
    }

    setSize (kW, kH);
    timerCallback();
    startTimerHz (30);
}

MainPanel::~MainPanel()
{
    stopTimer();
}

ModInfo MainPanel::modFor (const char* targetId) const
{
    return { proc.getLfoDepthFor (targetId), proc.engine.lfoValue.load() };
}

void MainPanel::setParam (const char* id, float v)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        prm->endChangeGesture();
    }
}

void MainPanel::chooseKey (int root, int scale)
{
    setParam (ids::keyAuto, 0.0f);
    setParam (ids::keyRoot, (float) root);
    setParam (ids::keyScale, (float) juce::jlimit (0, 2, scale));
}

juce::String MainPanel::statusText() const
{
    juce::String src;
    if (proc.getSourceMode() == 1)
    {
        int n = 0;
        for (int i = 0; i < BodenlegerProcessor::kSlots; ++i)
            if (proc.getSample (i) != nullptr && proc.apvts.getRawParameterValue (ids::slotLevel (i))->load() > 0.001f) ++n;
        src = n == 0 ? "no samples loaded" : juce::String (n) + (n == 1 ? " sample" : " samples");
    }
    else
        src = proc.isAudioRunning() ? "return, 7.5 s window" : "return, waiting for audio";

    const auto k = proc.getEffectiveKey();
    const bool autoOn = proc.apvts.getRawParameterValue (ids::keyAuto)->load() > 0.5f;
    const bool wash = proc.apvts.getRawParameterValue (ids::mode)->load() > 0.5f;
    const int noiseMode = juce::jlimit (0, 2, (int) proc.apvts.getRawParameterValue (ids::noiseMode)->load());
    return src + "  /  " + kNoteNames[k.root] + " " + kScaleNames[k.scale] + (autoOn ? " (auto)" : "")
         + ", " + kKeyModes[juce::jlimit (0, 2, proc.getKeyMode())]
         + "  /  " + (wash ? "wash" : "drone") + ", " + kNoiseModes[noiseMode] + " noise";
}

void MainPanel::timerCallback()
{
    ++tick;
    vu.setLevel (proc.engine.outputRms.load());
    spectrum.updateLive();
    spectrum.repaint();

    const int source = proc.getSourceMode();
    if (source != shownSource)
    {
        shownSource = source;
        returnPanel.setVisible (source == 0);
        for (auto& s : slots) s->setVisible (source == 1);
        capture.setButtonText (source == 0 ? "Capture now" : "Re-roll");
        repaint (rackTitleArea);
    }
    if (source == 0 && (tick & 1) == 0) returnPanel.repaint();
    if (source == 1) for (auto& s : slots) s->refresh();

    const auto k = proc.getEffectiveKey();
    if (scaleBox.getSelectedId() != k.scale + 1)
        scaleBox.setSelectedId (k.scale + 1, juce::dontSendNotification);
    const auto notes = bl::maskNotes (k, proc.getKeyMode());
    for (int i = 0; i < 12; ++i)
        keys[i]->setState (std::find (notes.begin(), notes.end(), i) != notes.end(), i == k.root);

    const bool b = proc.isBuilding();
    if (b != shownBuilding) { shownBuilding = b; repaint (buildingArea); }

    const auto st = statusText();
    if (st != shownStatus) { shownStatus = st; repaint (statusArea); }

    // modulation and breath indicators
    for (auto* kn : knobs) kn->refreshIndicators();
    const bool synced = proc.apvts.getRawParameterValue (ids::lfoSync)->load() > 0.5f;
    if (divisionKnob.isVisible() != synced) { divisionKnob.setVisible (synced); rateKnob.setVisible (! synced); }
    const float lfo = proc.isAudioRunning() ? proc.engine.lfoValue.load() : 0.0f;
    if (std::abs (lfo - shownLfo) > 0.02f) { shownLfo = lfo; repaint (lfoLampArea.expanded (6)); }
    if ((tick % 6) == 0) listenSwitch.repaint();
}

void MainPanel::resized()
{
    sourceSwitch.setBounds (486, 46, 196, 36);
    modeSwitch.setBounds (700, 46, 170, 36);
    vu.setBounds (kW - kEar - 20 - 144, 14, 144, 74);

    const int slotX = kEar + 16, slotW = kRackRight - kEar - 32;
    for (int i = 0; i < BodenlegerProcessor::kSlots; ++i)
        slots[(size_t) i]->setBounds (slotX, 140 + i * 98, slotW, 90);
    returnPanel.setBounds (slotX, 140, slotW, kBodyBottom - 150);
    rackTitleArea = { kEar, kHeaderH, kRackRight - kEar, 38 };

    const int sx = kRackRight + 22, sw = kW - kEar - 22 - sx;
    capture.setBounds (sx + sw - 110, 112, 110, 24);
    buildingArea = { sx + 150, 110, 200, 28 };
    spectrum.setBounds (sx + 2, 150, sw - 4, 248);

    int ky = 414;
    autoKey.setBounds (sx + 36, ky, 84, 30);
    scaleBox.setBounds (sx + 126, ky + 2, 118, 26);
    const int kx = sx + 254, kw = sx + sw - kx;
    for (int i = 0; i < 12; ++i)
        keys[i]->setBounds (kx + i * kw / 12, ky, kw / 12 - 4, 30);
    statusArea = { sx, 452, sw, 22 };

    const int kr = kBodyBottom + 14, knobW = (kW - 2 * kEar) / knobs.size();
    for (int i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (kEar + i * knobW, kr, knobW, 146);

    // character switches
    keyModeSwitch.setBounds (54, 746, 110, 92);
    noiseModeSwitch.setBounds (168, 746, 106, 92);
    breathModeSwitch.setBounds (278, 746, 110, 92);
    listenSwitch.setBounds (392, 746, 132, 92);

    // modulation
    lfoLampArea = { 690, 706, 11, 11 };
    lfoShapes.setBounds (552, 730, 182, 66);
    lfoSync.setBounds (552, 802, 182, 30);
    rateKnob.setBounds (742, 724, 92, 112);
    divisionKnob.setBounds (742, 724, 92, 112);
    depthKnob.setBounds (836, 724, 92, 112);
    for (int i = 0; i < targets.size(); ++i)
        targets[i]->setBounds (942, 726 + i * 25, 96, 22);
}

void MainPanel::paint (juce::Graphics& g)
{
    g.fillAll (col::page);

    // rack ears
    for (int side = 0; side < 2; ++side)
    {
        const auto ear = juce::Rectangle<int> (side == 0 ? 0 : kW - kEar, 0, kEar, kH);
        juce::ColourGradient grad (juce::Colour (0xff45403a), (float) ear.getX(), 0, juce::Colour (0xff2c2925), (float) ear.getRight(), 0, false);
        if (side == 1) grad = juce::ColourGradient (juce::Colour (0xff2c2925), (float) ear.getX(), 0, juce::Colour (0xff45403a), (float) ear.getRight(), 0, false);
        g.setGradientFill (grad);
        g.fillRect (ear);
        drawGrain (g, ear, 0.35f);
        const float cx = (float) ear.getCentreX();
        drawScrew (g, { cx, 30.0f }, side == 0 ? 0.4f : 1.47f);
        drawScrew (g, { cx, (float) kH - 30.0f }, side == 0 ? -1.06f : 0.21f);
    }

    // enamel panel
    const auto panel = juce::Rectangle<int> (kEar, 0, kW - 2 * kEar, kH);
    juce::ColourGradient enamel (col::cream2, 0, 0, col::cream, 0, (float) kH, false);
    g.setGradientFill (enamel);
    g.fillRect (panel);
    drawGrain (g, panel, 0.2f);
    for (int i = 0; i < 10; ++i)
    {
        g.setColour (juce::Colour::fromRGB (70, 45, 15).withAlpha (0.035f * (10 - i) / 10.0f));
        g.drawRect (panel.reduced (i), 1);
    }

    // dividers
    auto groove = [&] (int x0, int y0, int x1, int y1)
    {
        g.setColour (col::line);
        g.drawLine ((float) x0, (float) y0, (float) x1, (float) y1, 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        if (y0 == y1) g.drawLine ((float) x0, (float) y0 + 1.5f, (float) x1, (float) y1 + 1.5f, 1.0f);
        else g.drawLine ((float) x0 + 1.5f, (float) y0, (float) x1 + 1.5f, (float) y1, 1.0f);
    };
    groove (kEar, kHeaderH, kW - kEar, kHeaderH);
    groove (kRackRight, kHeaderH, kRackRight, kBodyBottom);
    groove (kEar, kBodyBottom, kW - kEar, kBodyBottom);
    groove (kEar, kKnobBottom, kW - kEar, kKnobBottom);
    groove (kCharRight, kKnobBottom, kCharRight, kH - 34);

    // nameplate
    const auto plate = juce::Rectangle<float> (56, 20, 232, 60);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (plate.translated (0, 2), 6);
    juce::ColourGradient chrome (juce::Colour (0xfff4efe4), 0, plate.getY(), juce::Colour (0xffe2dbcd), 0, plate.getBottom(), false);
    chrome.addColour (0.55, juce::Colour (0xffbdb5a4));
    g.setGradientFill (chrome);
    g.fillRoundedRectangle (plate, 6);
    g.setColour (juce::Colour (0xff8a8273));
    g.drawRoundedRectangle (plate, 6, 1);
    g.setFont (script (44.0f));
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.drawText ("Bodenleger", plate.translated (0, 1).withTrimmedBottom (4), juce::Justification::centred, false);
    g.setColour (juce::Colour (0xff7d1e0d));
    g.drawText ("Bodenleger", plate.withTrimmedBottom (4), juce::Justification::centred, false);
    drawScrew (g, { plate.getX() + 9, plate.getCentreY() }, 0.8f);
    drawScrew (g, { plate.getRight() - 9, plate.getCentreY() }, -0.3f);

    drawEngraved (g, "MODEL BL-1", { 302, 30, 160, 20 }, label (16.0f, 0.22f), juce::Justification::centredLeft, col::ink2);
    g.setFont (typewriter (13.0f));
    g.setColour (col::ink3);
    g.drawText ("Leftover pad generator", juce::Rectangle<float> (302, 52, 176, 20), juce::Justification::centredLeft, false);

    drawEngraved (g, "SOURCE", { 486, 22, 196, 20 }, label (12.5f, 0.2f), juce::Justification::centredLeft);
    drawEngraved (g, "MODE", { 700, 22, 170, 20 }, label (12.5f, 0.2f), juce::Justification::centredLeft);

    // rack
    drawEngraved (g, shownSource == 0 ? "RETURN" : "SAMPLES", rackTitleArea.toFloat().withTrimmedLeft (16).withTrimmedTop (8),
                  label (16.0f, 0.26f), juce::Justification::centredLeft, col::ink);

    // screen
    const int sx = kRackRight + 22;
    drawEngraved (g, "PAD SPECTRUM", { (float) sx, 110, 200, 28 }, label (16.0f, 0.26f), juce::Justification::centredLeft, col::ink);
    if (shownBuilding)
    {
        g.setFont (typewriter (14.0f));
        g.setColour (col::accent);
        g.drawText ("building pad...", buildingArea.toFloat(), juce::Justification::centredLeft, false);
    }
    drawBezel (g, spectrum.getBounds().toFloat(), 4);

    drawEngraved (g, "KEY", { (float) sx, 414, 34, 30 }, label (13.0f, 0.2f), juce::Justification::centredLeft);

    g.setFont (typewriter (14.0f));
    g.setColour (col::ink3);
    g.drawText (shownStatus, statusArea.toFloat(), juce::Justification::centredLeft, true);

    // legend
    float lx = (float) sx;
    const float ly = 482;
    auto chip = [&] (juce::Colour c, const juce::String& t)
    {
        g.setColour (c);
        g.fillRoundedRectangle (lx, ly + 5, 10, 10, 2);
        g.setColour (col::ink3);
        g.setFont (typewriter (13.0f));
        const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t);
        g.drawText (t, juce::Rectangle<float> (lx + 16, ly, w + 4, 20), juce::Justification::centredLeft, false);
        lx += w + 40;
    };
    chip (juce::Colour::fromRGB (150, 55, 25), "Raw smear");
    chip (juce::Colour::fromRGB (50, 26, 12), "In key");
    chip (juce::Colour::fromRGB (90, 95, 80), "Noise");
    chip (juce::Colour::fromRGB (120, 20, 5), "Output (live)");

    // character
    drawEngraved (g, "CHARACTER", { 54, 698, 200, 26 }, label (16.0f, 0.26f), juce::Justification::centredLeft, col::ink);
    const char* charTitles[] = { "IN KEY", "NOISE", "BREATH", "LISTEN" };
    const ToggleSwitch* charSwitches[] = { &keyModeSwitch, &noiseModeSwitch, &breathModeSwitch, &listenSwitch };
    for (int i = 0; i < 4; ++i)
        drawEngraved (g, charTitles[i], charSwitches[i]->getBounds().toFloat().withY (724).withHeight (18),
                      label (12.5f, 0.2f), juce::Justification::centredLeft);

    // modulation
    drawEngraved (g, "MODULATION", { 552, 698, 200, 26 }, label (16.0f, 0.26f), juce::Justification::centredLeft, col::ink);
    {
        const auto lamp = lfoLampArea.toFloat();
        const float v = 0.5f + 0.5f * shownLfo;
        g.setColour (col::lampOn.withAlpha (0.35f * v));
        g.fillEllipse (lamp.expanded (4 * v));
        g.setColour (col::lampOff.interpolatedWith (col::lampOn, v));
        g.fillEllipse (lamp);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawEllipse (lamp, 0.8f);
    }
    drawEngraved (g, "SEND TO", { (float) targets[0]->getX(), 698, 120, 26 }, label (12.5f, 0.2f), juce::Justification::centredLeft);

    // footer
    g.setFont (typewriter (13.0f));
    g.setColour (col::ink3);
    g.drawText ("Bodenleger was Offergeld's idea: a floor laid under the track from its own leftovers.",
                juce::Rectangle<float> ((float) kEar + 20, (float) kH - 28, 640, 22), juce::Justification::centredLeft, false);
    g.drawText ("v" + juce::String (JucePlugin_VersionString),
                juce::Rectangle<float> ((float) kW - kEar - 120, (float) kH - 28, 100, 22), juce::Justification::centredRight, false);
}

//==============================================================================
BodenlegerEditor::BodenlegerEditor (BodenlegerProcessor& p)
    : AudioProcessorEditor (&p), proc (p), panel (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (panel);

    const double scale = juce::jlimit (0.6, 1.6, (double) p.apvts.state.getProperty ("uiScale", 0.88));
    setResizable (true, true);
    setResizeLimits ((int) (kW * 0.6), (int) (kH * 0.6), (int) (kW * 1.6), (int) (kH * 1.6));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kW / kH);
    setSize (juce::roundToInt (kW * scale), juce::roundToInt (kH * scale));
}

BodenlegerEditor::~BodenlegerEditor()
{
    setLookAndFeel (nullptr);
}

void BodenlegerEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::page);
}

void BodenlegerEditor::resized()
{
    const float scale = (float) getWidth() / (float) kW;
    panel.setBounds (0, 0, kW, kH);
    panel.setTransform (juce::AffineTransform::scale (scale));
    proc.apvts.state.setProperty ("uiScale", scale, nullptr);
}
