#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
class BodenlegerProcessor::BuildThread : public juce::Thread
{
public:
    explicit BuildThread (BodenlegerProcessor& p) : juce::Thread ("Bodenleger builder"), owner (p) {}

    void run() override
    {
        while (! threadShouldExit())
        {
            wait (50);

            while (auto* r = owner.engine.takeRetired())
                delete r;

            const auto now = juce::Time::getMillisecondCounter();
            const bool returnMode = owner.getSourceMode() == 0;
            bool due = owner.buildRequested.load()
                       && (owner.forceBuild.load() || now - owner.requestTime.load() > 350);

            // Return mode needs some captured audio before it can build anything.
            if (due && returnMode && owner.engine.inputWritten() < (juce::int64) owner.engine.getSampleRate())
                due = false;

            // Follow: recapture every 8 seconds while audio is running.
            if (! due && returnMode && owner.param (ids::follow) > 0.5f && owner.isAudioRunning()
                && now - owner.lastBuildTime.load() > 8000
                && owner.engine.inputWritten() > (juce::int64) (owner.engine.getSampleRate() * 3))
                due = true;

            if (due)
            {
                owner.buildRequested = false;
                owner.forceBuild = false;
                owner.doBuild();
                owner.lastBuildTime = juce::Time::getMillisecondCounter();
            }
        }
    }

private:
    BodenlegerProcessor& owner;
};

//==============================================================================
BodenlegerProcessor::BodenlegerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Bodenleger", createLayout())
{
    formats.registerBasicFormats();

    for (auto* id : { ids::source, ids::keyAuto, ids::keyRoot, ids::keyScale, ids::returnAir })
        apvts.addParameterListener (id, this);
    for (int i = 0; i < kSlots; ++i)
    {
        apvts.addParameterListener (ids::slotLevel (i), this);
        apvts.addParameterListener (ids::slotAir (i), this);
    }

    buildThread = std::make_unique<BuildThread> (*this);
    buildThread->startThread (juce::Thread::Priority::low);
}

BodenlegerProcessor::~BodenlegerProcessor()
{
    alive->store (false);
    buildThread->stopThread (4000);
}

juce::AudioProcessorValueTreeState::ParameterLayout BodenlegerProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto unit = NormalisableRange<float> (0.0f, 1.0f, 0.001f);

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::source, 1 }, "Source", StringArray { "Return", "Samples" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::mode, 1 }, "Mode", StringArray { "Drone", "Wash" }, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::keyAuto, 1 }, "Key Auto", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::keyRoot, 1 }, "Key Root",
        StringArray { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" }, 9));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::keyScale, 1 }, "Key Scale",
        StringArray { "Minor", "Major", "Root + 5th" }, 0));

    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::tonal, 1 }, "Tonal", unit, 0.6f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::noise, 1 }, "Noise", unit, 0.3f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return v < 0.02f ? String ("off") : String (roundToInt (20.0f * std::log10 (v * v * 0.9f))) + " dB"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::drift, 1 }, "Drift", unit, 0.4f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return String (0.02f + v * 0.13f, 2) + " Hz"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::color, 1 }, "Color", unit, 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            const float f = 600.0f * std::pow (30.0f, v);
            return f >= 1000.0f ? String (f / 1000.0f, f >= 10000.0f ? 0 : 1) + " kHz" : String (roundToInt (f)) + " Hz";
        })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::breath, 1 }, "Breath", unit, 0.3f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::mix, 1 }, "Mix", unit, 1.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return String (roundToInt (v * 100.0f)) + " % pad"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::returnAir, 1 }, "Return Tone/Air", unit, 0.4f));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::follow, 1 }, "Follow", true));

    for (int i = 0; i < kSlots; ++i)
    {
        const String n = "Slot " + String (i + 1);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::slotLevel (i), 1 }, n + " Level", unit, 0.8f,
            AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::slotAir (i), 1 }, n + " Tone/Air", unit, 0.4f));
    }
    return layout;
}

void BodenlegerProcessor::parameterChanged (const juce::String&, float)
{
    requestBuild (false);
}

void BodenlegerProcessor::requestBuild (bool immediate)
{
    requestTime = juce::Time::getMillisecondCounter();
    if (immediate) forceBuild = true;
    buildRequested = true;
}

//==============================================================================
bool BodenlegerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void BodenlegerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const double old = hostRate.exchange (sampleRate);
    engine.prepare (sampleRate, samplesPerBlock);
    requestBuild (true);

    if (std::abs (old - sampleRate) > 1.0)
    {
        // Samples are stored at the host rate; reload them for the new rate.
        auto flag = alive;
        juce::MessageManager::callAsync ([this, flag]
        {
            if (! flag->load()) return;
            for (int i = 0; i < kSlots; ++i)
            {
                juce::File f;
                double rate = 0;
                {
                    const juce::ScopedLock sl (slotLock);
                    f = slotFiles[(size_t) i];
                    rate = slotRates[(size_t) i];
                }
                if (f.existsAsFile() && std::abs (rate - hostRate.load()) > 1.0)
                    loadSample (i, f);
            }
        });
    }
}

bool BodenlegerProcessor::isAudioRunning() const
{
    return juce::Time::getMillisecondCounter() - lastAudioTime.load() < 1000;
}

void BodenlegerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2) return;

    if (getTotalNumInputChannels() == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    else if (getTotalNumInputChannels() == 0)
        buffer.clear();

    bl::EngineParams p;
    p.source = (int) param (ids::source);
    p.mode   = (int) param (ids::mode);
    p.tonal  = param (ids::tonal);
    p.noise  = param (ids::noise);
    p.drift  = param (ids::drift);
    p.color  = param (ids::color);
    p.breath = param (ids::breath);
    p.mix    = param (ids::mix);
    p.key    = getEffectiveKey();

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    engine.process (l, r, l, r, n, p);

    lastAudioTime = juce::Time::getMillisecondCounter();
}

bl::Key BodenlegerProcessor::getEffectiveKey() const
{
    if (param (ids::keyAuto) > 0.5f)
        return { detectedRoot.load(), detectedScale.load() };
    return { (int) param (ids::keyRoot), (int) param (ids::keyScale) };
}

//==============================================================================
void BodenlegerProcessor::doBuild()
{
    const double sr = engine.getSampleRate();
    if (sr <= 0) return;

    std::vector<bl::SourceSpectra> sources;
    std::vector<std::pair<std::shared_ptr<const bl::SampleData>, float>> grains;
    const bool samplesMode = getSourceMode() == 1;

    if (samplesMode)
    {
        const juce::ScopedLock sl (slotLock);
        for (int i = 0; i < kSlots; ++i)
        {
            const float level = param (ids::slotLevel (i).toRawUTF8());
            if (slots[(size_t) i] == nullptr || level < 0.001f) continue;
            sources.push_back ({ slots[(size_t) i]->spectra, level, 1.0f - param (ids::slotAir (i).toRawUTF8()) });
            grains.push_back ({ slots[(size_t) i], level });
        }
    }
    else
    {
        const int win = (int) (2.5 * sr);
        std::vector<float> recent ((size_t) win * 3);
        engine.copyRecentInput (recent.data(), win * 3);
        bl::SourceSpectra s;
        for (int k = 0; k < bl::kSnapshots; ++k)
            s.spectra[(size_t) k] = bl::spectrumOf (recent.data() + k * win, win);
        bl::normaliseSet (s.spectra);
        s.level = 1.0f;
        s.tone = 1.0f - param (ids::returnAir);
        sources.push_back (std::move (s));
    }

    const bool autoKey = param (ids::keyAuto) > 0.5f;
    const bl::Key manual { (int) param (ids::keyRoot), (int) param (ids::keyScale) };

    building = true;
    auto result = builder.build (sources, std::move (grains), autoKey, manual, sr);
    building = false;

    if (result.set != nullptr)
    {
        if (autoKey)
        {
            detectedRoot = result.key.root;
            detectedScale = result.key.scale;
        }
        delete engine.submit (result.set);

        auto d = std::make_shared<DisplayData>();
        d->snapshots = std::move (result.snapshots);
        d->mask = std::move (result.displayMask);
        d->key = result.key;
        d->sampleRate = sr;
        d->sourceCount = (int) sources.size();
        const juce::SpinLock::ScopedLockType sl (displayLock);
        display = std::move (d);
    }
    else if (samplesMode && sources.empty())
    {
        delete engine.submit (new bl::LayerSet()); // silence
        const juce::SpinLock::ScopedLockType sl (displayLock);
        display.reset();
    }
}

std::shared_ptr<const DisplayData> BodenlegerProcessor::getDisplay() const
{
    const juce::SpinLock::ScopedLockType sl (displayLock);
    return display;
}

//==============================================================================
bool BodenlegerProcessor::loadSample (int slot, const juce::File& file)
{
    if (slot < 0 || slot >= kSlots) return false;
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0) return false;

    const double fileRate = reader->sampleRate;
    const int n = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (fileRate * 60.0));
    const int chans = reader->numChannels > 1 ? 2 : 1;
    juce::AudioBuffer<float> tmp (chans, n + 16);
    tmp.clear();
    reader->read (&tmp, 0, n, 0, true, chans > 1);

    const double target = hostRate.load();
    const double ratio = fileRate / target;
    const int outLen = std::max (64, (int) (n / ratio));

    auto resample = [&] (int ch)
    {
        std::vector<float> out ((size_t) outLen, 0.0f);
        if (std::abs (ratio - 1.0) < 1e-6)
        {
            std::copy (tmp.getReadPointer (ch), tmp.getReadPointer (ch) + std::min (n, outLen), out.begin());
        }
        else
        {
            juce::LagrangeInterpolator interp;
            interp.process (ratio, tmp.getReadPointer (ch), out.data(), outLen, n + 16, 0);
        }
        return out;
    };

    auto left = resample (0);
    auto right = chans > 1 ? resample (1) : left;
    auto data = bl::makeSampleData (file.getFileNameWithoutExtension().toStdString(), std::move (left), std::move (right));

    {
        const juce::ScopedLock sl (slotLock);
        slots[(size_t) slot] = std::move (data);
        slotFiles[(size_t) slot] = file;
        slotRates[(size_t) slot] = target;
    }
    requestBuild (true);
    return true;
}

void BodenlegerProcessor::clearSample (int slot)
{
    if (slot < 0 || slot >= kSlots) return;
    {
        const juce::ScopedLock sl (slotLock);
        slots[(size_t) slot].reset();
        slotFiles[(size_t) slot] = juce::File();
    }
    requestBuild (true);
}

std::shared_ptr<const bl::SampleData> BodenlegerProcessor::getSample (int slot) const
{
    const juce::ScopedLock sl (slotLock);
    return slots[(size_t) slot];
}

//==============================================================================
void BodenlegerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    auto* slotsXml = xml->createNewChildElement ("Slots");
    {
        const juce::ScopedLock sl (slotLock);
        for (int i = 0; i < kSlots; ++i)
            slotsXml->setAttribute ("path" + juce::String (i), slotFiles[(size_t) i].getFullPathName());
    }
    copyXmlToBinary (*xml, destData);
}

void BodenlegerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr) return;

    juce::StringArray paths;
    if (auto* slotsXml = xml->getChildByName ("Slots"))
    {
        for (int i = 0; i < kSlots; ++i)
            paths.add (slotsXml->getStringAttribute ("path" + juce::String (i)));
        xml->removeChildElement (slotsXml, true);
    }

    if (xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));

    auto flag = alive;
    juce::MessageManager::callAsync ([this, flag, paths]
    {
        if (! flag->load()) return;
        for (int i = 0; i < paths.size() && i < kSlots; ++i)
        {
            const juce::File f (paths[i]);
            if (paths[i].isNotEmpty() && f.existsAsFile()) loadSample (i, f);
            else clearSample (i);
        }
    });
    requestBuild (true);
}

juce::AudioProcessorEditor* BodenlegerProcessor::createEditor()
{
    return new BodenlegerEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BodenlegerProcessor();
}
