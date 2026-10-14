#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UI/Translations.h"

namespace sm
{
//==============================================================================
juce::var PreviewSettings::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("syncTempo", syncTempo);
    o->setProperty ("keepPitch", keepPitch);
    o->setProperty ("syncKey", syncKey);
    o->setProperty ("targetKey", targetKey.toInt());
    o->setProperty ("transpose", transpose);
    o->setProperty ("direction", (int) direction);
    o->setProperty ("hp", highPassHz);
    o->setProperty ("lp", lowPassHz);
    o->setProperty ("fadeIn", fadeInMs);
    o->setProperty ("fadeOut", fadeOutMs);
    o->setProperty ("gain", gainDb);
    o->setProperty ("normalize", normalize);
    o->setProperty ("loop", loop);
    o->setProperty ("quantize", quantize);
    o->setProperty ("autoPlay", autoPlay);
    o->setProperty ("dragProcessed", dragProcessed);
    return juce::var (o);
}

void PreviewSettings::fromVar (const juce::var& v)
{
    if (! v.isObject()) return;
    syncTempo = v.getProperty ("syncTempo", syncTempo);
    keepPitch = v.getProperty ("keepPitch", keepPitch);
    syncKey = v.getProperty ("syncKey", syncKey);
    auto k = MusicalKey::fromInt (v.getProperty ("targetKey", targetKey.toInt()));
    if (k.isValid()) targetKey = k;
    transpose = v.getProperty ("transpose", transpose);
    direction = (ProcessSettings::Direction) (int) v.getProperty ("direction", (int) direction);
    highPassHz = v.getProperty ("hp", highPassHz);
    lowPassHz = v.getProperty ("lp", lowPassHz);
    fadeInMs = v.getProperty ("fadeIn", fadeInMs);
    fadeOutMs = v.getProperty ("fadeOut", fadeOutMs);
    gainDb = v.getProperty ("gain", gainDb);
    normalize = v.getProperty ("normalize", normalize);
    loop = v.getProperty ("loop", loop);
    quantize = v.getProperty ("quantize", quantize);
    autoPlay = v.getProperty ("autoPlay", autoPlay);
    dragProcessed = v.getProperty ("dragProcessed", dragProcessed);
}

//==============================================================================
SampleManagerProcessor::SampleManagerProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formats.registerBasicFormats();
    applyLanguage (getDatabase().getSetting ("language", "en"));
    loadSettingsFromDatabase();
    midiLearn.deserialise (getDatabase().getSetting ("midiMappings"));
    midiLearn.onMappingsChanged = [this] { getDatabase().setSetting ("midiMappings", midiLearn.serialise()); };
    tempoSource.setTempo (getDatabase().getSetting ("manualTempo", "120").getDoubleValue());
    startTimerHz (20);
}

SampleManagerProcessor::~SampleManagerProcessor()
{
    stopTimer();
    renderPool.removeAllJobs (true, 5000);
    saveSettingsToDatabase();
}

void SampleManagerProcessor::loadSettingsFromDatabase()
{
    settings.fromVar (juce::JSON::parse (getDatabase().getSetting ("previewSettings")));
}

void SampleManagerProcessor::saveSettingsToDatabase()
{
    getDatabase().setSetting ("previewSettings", juce::JSON::toString (settings.toVar(), true));
}

bool SampleManagerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void SampleManagerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const bool rateChanged = std::abs (currentSampleRate.load() - sampleRate) > 1.0;
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    if (rateChanged) settingsDirty = true; // re-render at the new rate
}

void SampleManagerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    TransportState t;
    if (isStandalone())
    {
        t = tempoSource.advance (buffer.getNumSamples(), getSampleRate(), getLatencySamples() / juce::jmax (1.0, getSampleRate()));
    }
    else if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) { t.bpm = *bpm; t.valid = true; }
            if (auto ppq = pos->getPpqPosition()) t.beatAtBlockStart = *ppq;
            t.playing = pos->getIsPlaying();
        }
    }
    if (t.valid) lastTempo = t.bpm;
    lastPlaying = t.playing;

    midiLearn.pushFromAudioThread (midi);
    engine.process (buffer, midi, t);
}

//==============================================================================
RenderedSample::Ptr SampleManagerProcessor::makeRendered (const juce::AudioBuffer<float>& src, double rate, const ProcessSettings& ps,
                                                          double outRate, const SampleRow& row, int gen) const
{
    RenderedSample::Ptr rs = new RenderedSample();
    rs->buffer = SampleRenderer::render (src, rate, ps, outRate);
    rs->outputRate = outRate;
    rs->isLoop = row.isLoop;
    rs->sampleId = row.id;
    rs->generation = gen;
    if (row.isLoop && ps.sourceBpm > 0)
    {
        const double secs = src.getNumSamples() / rate * (ps.endNorm - ps.startNorm);
        rs->lengthInBeats = std::round (secs * ps.sourceBpm / 60.0 * 4.0) / 4.0;
        if (ps.direction == ProcessSettings::PingPong) rs->lengthInBeats *= 2.0;
    }
    return rs;
}

void SampleManagerProcessor::selectSample (const SampleRow& row, bool play)
{
    selected = row;
    trimStart = 0.0;
    trimEnd = 1.0;
    const int gen = ++renderGeneration;
    const auto ps = buildProcessSettings (row);
    const double outRate = currentSampleRate.load();

    engine.setLooping (settings.loop && row.isLoop);
    engine.setQuantize ((PreviewEngine::Quantize) settings.quantize);
    if (play) engine.stop();
    renderedTempo = lastTempo.load();

    juce::WeakReference<SampleManagerProcessor> weakThis (this);
    renderPool.addJob ([this, weakThis, row, gen, ps, outRate, play]
    {
        auto buf = std::make_shared<juce::AudioBuffer<float>>();
        double rate = 0;
        if (! SampleRenderer::load (formats, row.file(), *buf, rate)) return;
        if (gen != renderGeneration.load()) return;
        {
            const juce::ScopedLock sl (sourceLock);
            source = buf;
            sourceRate = rate;
        }
        auto rs = makeRendered (*buf, rate, ps, outRate, row, gen);
        if (gen != renderGeneration.load()) return;
        engine.setSample (rs, false);
        if (play) engine.play();
        juce::MessageManager::callAsync ([this, gen, alive = weakThis]
        {
            if (alive != nullptr && gen == renderGeneration.load() && onSampleLoaded) onSampleLoaded();
        });
    });
}

std::shared_ptr<const juce::AudioBuffer<float>> SampleManagerProcessor::getSourceAudio() const
{
    const juce::ScopedLock sl (sourceLock);
    return source;
}

ProcessSettings SampleManagerProcessor::buildProcessSettings (const SampleRow& row) const
{
    ProcessSettings p;
    p.startNorm = trimStart;
    p.endNorm = trimEnd;
    p.direction = settings.direction;
    if (settings.syncTempo && row.isLoop && row.bpm > 0)
    {
        p.sourceBpm = row.bpm;
        p.targetBpm = lastTempo.load();
    }
    p.keepPitch = settings.keepPitch;
    p.transposeSemitones = settings.transpose;
    if (settings.syncKey && row.key.isValid() && settings.targetKey.isValid())
        p.transposeSemitones += theory::semitonesBetween (row.key, settings.targetKey);
    p.highPassHz = settings.highPassHz;
    p.lowPassHz = settings.lowPassHz;
    p.fadeInMs = settings.fadeInMs;
    p.fadeOutMs = settings.fadeOutMs;
    p.gainDb = settings.gainDb;
    p.normalize = settings.normalize;
    return p;
}

void SampleManagerProcessor::requestRender (bool keepPlaying)
{
    auto src = getSourceAudio();
    if (src == nullptr || ! hasSelection()) return;
    const int gen = ++renderGeneration;
    const auto ps = buildProcessSettings (selected);
    const double outRate = currentSampleRate.load();
    const double rate = sourceRate;
    const auto row = selected;
    engine.setLooping (settings.loop && row.isLoop);
    engine.setQuantize ((PreviewEngine::Quantize) settings.quantize);
    renderedTempo = lastTempo.load();

    renderPool.addJob ([this, src, gen, ps, outRate, rate, row, keepPlaying]
    {
        if (gen != renderGeneration.load()) return;
        auto rs = makeRendered (*src, rate, ps, outRate, row, gen);
        if (gen != renderGeneration.load()) return;
        engine.setSample (rs, keepPlaying);
    });
}

void SampleManagerProcessor::settingsChanged()
{
    settingsDirty = true;
    settingsChangedAt = juce::Time::getMillisecondCounter();
    engine.setLooping (settings.loop && selected.isLoop);
    engine.setQuantize ((PreviewEngine::Quantize) settings.quantize);
}

void SampleManagerProcessor::setTrim (double s, double e)
{
    trimStart = juce::jlimit (0.0, 1.0, s);
    trimEnd = juce::jlimit (trimStart, 1.0, e);
    settingsChanged();
}

void SampleManagerProcessor::togglePlay()
{
    if (engine.isPlaying()) engine.stop();
    else if (hasSelection())
    {
        if (engine.getSample() == nullptr) selectSample (selected, true);
        else engine.play();
    }
}

juce::File SampleManagerProcessor::renderSelectedToFile()
{
    if (! hasSelection()) return {};
    auto src = getSourceAudio();
    double rate = sourceRate;
    if (src == nullptr)
    {
        auto b = std::make_shared<juce::AudioBuffer<float>>();
        if (! SampleRenderer::load (formats, selected.file(), *b, rate)) return selected.file();
        src = b;
    }
    const auto ps = buildProcessSettings (selected);
    if (ps.isNeutral()) return selected.file();
    auto rendered = SampleRenderer::render (*src, rate, ps, rate); // keep the file's own rate

    juce::String name = selected.filename;
    if (ps.targetBpm > 0) name << " " << juce::String (ps.targetBpm, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") << "bpm";
    if (ps.transposeSemitones != 0.0)
    {
        if (settings.syncKey && settings.targetKey.isValid()) name << " " << settings.targetKey.toString();
        else name << " " << (ps.transposeSemitones > 0 ? "+" : "") << juce::String ((int) ps.transposeSemitones) << "st";
    }
    if (ps.direction == ProcessSettings::Reverse) name << " rev";
    if (ps.direction == ProcessSettings::PingPong) name << " pingpong";
    auto file = paths::renderDirectory().getChildFile (juce::File::createLegalFileName (name) + ".wav");
    SampleRenderer::writeWav (rendered, rate, file, 24);
    return file;
}

//==============================================================================
void SampleManagerProcessor::timerCallback()
{
    engine.releaseUnused();

    midiLearn.dispatch ([this] (MidiLearn::Action a, float v)
    {
        switch (a)
        {
            case MidiLearn::Action::HighPass: settings.highPassHz = v < 0.01f ? 0.0 : 20.0 * std::pow (1000.0, (double) v); settingsChanged(); break;
            case MidiLearn::Action::LowPass:  settings.lowPassHz  = v > 0.99f ? 0.0 : 20.0 * std::pow (1000.0, (double) v); settingsChanged(); break;
            case MidiLearn::Action::FadeIn:   settings.fadeInMs  = v * 2000.0; settingsChanged(); break;
            case MidiLearn::Action::FadeOut:  settings.fadeOutMs = v * 2000.0; settingsChanged(); break;
            case MidiLearn::Action::Gain:     settings.gainDb = -36.0 + v * 48.0; settingsChanged(); break;
            case MidiLearn::Action::PlayStop: togglePlay(); break;
            case MidiLearn::Action::None: case MidiLearn::Action::Random: case MidiLearn::Action::RandomBack:
            case MidiLearn::Action::Next: case MidiLearn::Action::Previous: case MidiLearn::Action::NumActions: break;
        }
        if (onMidiAction) onMidiAction (a, v);
    });

    const auto now = juce::Time::getMillisecondCounter();

    // follow tempo changes of the host / Link session (debounced)
    if (hasSelection() && settings.syncTempo && selected.isLoop && std::abs (lastTempo.load() - renderedTempo) > 0.05)
    {
        if (tempoChangedAt == 0) tempoChangedAt = now;
        else if (now - tempoChangedAt > 200) { tempoChangedAt = 0; requestRender (true); }
    }
    else tempoChangedAt = 0;

    if (settingsDirty.load() && now - settingsChangedAt > 60)
    {
        settingsDirty = false;
        requestRender (true);
    }
}

//==============================================================================
juce::AudioProcessorEditor* SampleManagerProcessor::createEditor()
{
    return new SampleManagerEditor (*this);
}

void SampleManagerProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement xml ("SampleManagerState");
    xml.setAttribute ("version", 1);
    xml.setAttribute ("ui", uiStateJson);
    xml.setAttribute ("settings", juce::JSON::toString (settings.toVar(), true));
    xml.setAttribute ("selected", juce::String (selected.id));
    xml.setAttribute ("w", editorWidth);
    xml.setAttribute ("h", editorHeight);
    copyXmlToBinary (xml, dest);
}

void SampleManagerProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        uiStateJson = xml->getStringAttribute ("ui");
        settings.fromVar (juce::JSON::parse (xml->getStringAttribute ("settings")));
        editorWidth = xml->getIntAttribute ("w", editorWidth);
        editorHeight = xml->getIntAttribute ("h", editorHeight);
        const auto id = xml->getStringAttribute ("selected").getLargeIntValue();
        if (id != 0)
            if (auto row = getDatabase().sample (id))
                selectSample (*row, false);
    }
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new sm::SampleManagerProcessor();
}
