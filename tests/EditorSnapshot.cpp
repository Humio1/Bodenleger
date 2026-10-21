// Renders the plugin window to a PNG without a host, for quick visual checks.
// Usage: EditorSnapshot <out.png> [sample.wav ...]

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <thread>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png"));

    BodenlegerProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    const bool samples = argc > 2;
    if (auto* p = proc.apvts.getParameter (ids::source)) p->setValueNotifyingHost (samples ? 1.0f : 0.0f);
    for (int i = 2; i < argc && i < 6; ++i)
        proc.loadSample (i - 2, juce::File::getCurrentWorkingDirectory().getChildFile (argv[i]));

    // feed audio so the engine and builder have something to work with
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    double phase = 0;
    for (int b = 0; b < 48000 * 10 / 512; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            const float v = 0.15f * (float) (std::sin (phase) + std::sin (phase * 1.189) + std::sin (phase * 1.498));
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
            phase += 2 * juce::MathConstants<double>::pi * 220.0 / 48000.0;
        }
        proc.processBlock (buf, midi);
        if (b % 40 == 0) std::this_thread::sleep_for (std::chrono::milliseconds (20));
    }
    std::this_thread::sleep_for (std::chrono::milliseconds (1500));
    for (int b = 0; b < 200; ++b) proc.processBlock (buf, midi);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    for (int i = 0; i < 10; ++i) proc.processBlock (buf, midi);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::FileOutputStream os (out);
    os.setPosition (0);
    os.truncate();
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    ed.reset();
    return 0;
}
