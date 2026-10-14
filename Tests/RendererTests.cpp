#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Audio/SampleRenderer.h"
#include "Audio/PreviewEngine.h"
#include "Audio/MidiLearn.h"
#include "TestSignals.h"

using namespace sm;

struct RendererTests : juce::UnitTest
{
    RendererTests() : juce::UnitTest ("Renderer + preview", "SampleManager") {}

    void runTest() override
    {
        const double sr = 44100.0;
        auto loop = test::drumLoop (120.0, 2, sr); // 4 s

        beginTest ("neutral render keeps length and content");
        {
            ProcessSettings s;
            auto out = SampleRenderer::render (loop, sr, s, sr);
            expectEquals (out.getNumChannels(), 2);
            expectEquals (out.getNumSamples(), loop.getNumSamples());
            expectWithinAbsoluteError (out.getSample (0, 1000), loop.getSample (0, 1000), 1.0e-6f);
        }
        beginTest ("tempo sync is sample exact (120 -> 140 BPM)");
        {
            ProcessSettings s; s.sourceBpm = 120; s.targetBpm = 140;
            auto out = SampleRenderer::render (loop, sr, s, sr);
            const int expected = (int) std::llround (loop.getNumSamples() * 120.0 / 140.0);
            expectEquals (out.getNumSamples(), expected);
            expect (out.getMagnitude (0, out.getNumSamples()) > 0.1f);
        }
        beginTest ("varispeed + sample-rate conversion");
        {
            ProcessSettings s; s.sourceBpm = 120; s.targetBpm = 60; s.keepPitch = false;
            auto out = SampleRenderer::render (loop, sr, s, 48000.0);
            const int expected = (int) std::llround (loop.getNumSamples() * 2.0 * 48000.0 / sr);
            expectEquals (out.getNumSamples(), expected);
        }
        beginTest ("trim, reverse, ping-pong");
        {
            ProcessSettings s; s.startNorm = 0.25; s.endNorm = 0.75;
            auto out = SampleRenderer::render (loop, sr, s, sr);
            expectEquals (out.getNumSamples(), loop.getNumSamples() / 2);
            s = {}; s.direction = ProcessSettings::Reverse;
            out = SampleRenderer::render (loop, sr, s, sr);
            expectWithinAbsoluteError (out.getSample (0, 0), loop.getSample (0, loop.getNumSamples() - 1), 1.0e-6f);
            s.direction = ProcessSettings::PingPong;
            out = SampleRenderer::render (loop, sr, s, sr);
            expectEquals (out.getNumSamples(), loop.getNumSamples() * 2);
        }
        beginTest ("fades, normalise, gain, filters");
        {
            ProcessSettings s; s.fadeInMs = 50; s.fadeOutMs = 50; s.normalize = true;
            auto out = SampleRenderer::render (loop, sr, s, sr);
            expectWithinAbsoluteError (out.getSample (0, 0), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (out.getSample (0, out.getNumSamples() - 1), 0.0f, 1.0e-3f);
            expectWithinAbsoluteError (out.getMagnitude (0, out.getNumSamples()), 0.989f, 0.01f);
            s = {}; s.gainDb = -6.0;
            out = SampleRenderer::render (loop, sr, s, sr);
            expectWithinAbsoluteError (out.getMagnitude (0, out.getNumSamples()) / loop.getMagnitude (0, loop.getNumSamples()), 0.501f, 0.01f);
            s = {}; s.lowPassHz = 200;
            out = SampleRenderer::render (test::noiseBurst (1.0, sr, 1.0f), sr, s, sr);
            auto ref = test::noiseBurst (1.0, sr, 1.0f);
            expect (out.getRMSLevel (0, 0, out.getNumSamples()) < ref.getRMSLevel (0, 0, ref.getNumSamples()) * 0.5f);
        }
        beginTest ("pitch shift keeps length");
        {
            ProcessSettings s; s.transposeSemitones = 3;
            auto out = SampleRenderer::render (test::pluck (60, 1.0, sr), sr, s, sr);
            expectEquals (out.getNumSamples(), (int) sr);
        }
        beginTest ("wav export roundtrip");
        {
            auto f = juce::File::createTempFile (".wav");
            expect (SampleRenderer::writeWav (loop, sr, f));
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            juce::AudioBuffer<float> back; double rate = 0;
            expect (SampleRenderer::load (fm, f, back, rate));
            expectEquals (rate, sr);
            expectEquals (back.getNumSamples(), loop.getNumSamples());
            f.deleteFile();
        }
        beginTest ("quantise boundary maths");
        {
            // at 120 BPM a beat is 22050 samples; 0.5 beats before the next beat -> 11025
            expectEquals (PreviewEngine::samplesUntilBoundary (3.5, 120, sr, 1.0, 512), -1);
            expectEquals (PreviewEngine::samplesUntilBoundary (3.5, 120, sr, 1.0, 20000), 11025);
            expectEquals (PreviewEngine::samplesUntilBoundary (4.0, 120, sr, 4.0, 512), 0);
        }
        beginTest ("preview engine plays, loops and responds to MIDI");
        {
            PreviewEngine eng;
            eng.prepare (sr, 512);
            RenderedSample::Ptr rs = new RenderedSample();
            rs->buffer = SampleRenderer::render (test::pluck (60, 0.1, sr), sr, {}, sr);
            rs->outputRate = sr;
            eng.setSample (rs, false);
            eng.setLooping (false);
            eng.play();
            juce::AudioBuffer<float> out (2, 512);
            juce::MidiBuffer midi;
            TransportState t;
            out.clear(); eng.process (out, midi, t);
            expect (out.getMagnitude (0, 512) > 0.01f);
            expect (eng.isPlaying());
            for (int i = 0; i < 20; ++i) { out.clear(); eng.process (out, midi, t); }
            expect (! eng.isPlaying()); // one-shot finished
            eng.setLooping (true);
            eng.play();
            for (int i = 0; i < 20; ++i) { out.clear(); eng.process (out, midi, t); }
            expect (eng.isPlaying());
            eng.stop();
            for (int i = 0; i < 4; ++i) { out.clear(); eng.process (out, midi, t); }
            expect (! eng.isPlaying());
            midi.addEvent (juce::MidiMessage::noteOn (1, 72, 1.0f), 10);
            out.clear(); eng.process (out, midi, t);
            expect (out.getMagnitude (0, 10, 500) > 0.01f);
            expectEquals (out.getSample (0, 5), 0.0f);
        }
        beginTest ("midi learn");
        {
            MidiLearn ml;
            ml.startLearning (MidiLearn::Action::Random);
            juce::MidiBuffer mb; mb.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 0);
            ml.pushFromAudioThread (mb);
            ml.dispatch ([] (MidiLearn::Action, float) {});
            expectEquals (ml.controllerFor (MidiLearn::Action::Random), 20);
            int fired = 0;
            ml.pushFromAudioThread (mb);
            ml.dispatch ([&] (MidiLearn::Action a, float) { if (a == MidiLearn::Action::Random) ++fired; });
            expectEquals (fired, 1);
            MidiLearn copy; copy.deserialise (ml.serialise());
            expectEquals (copy.controllerFor (MidiLearn::Action::Random), 20);
        }
    }
};

static RendererTests rendererTests;
