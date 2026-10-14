#include <juce_core/juce_core.h>
#include "Core/AudioAnalyzer.h"
#include "Core/FilenameParser.h"
#include "Core/MusicTheory.h"
#include "TestSignals.h"

using namespace sm;

struct MusicTheoryTests : juce::UnitTest
{
    MusicTheoryTests() : juce::UnitTest ("Music theory", "SampleManager") {}
    void runTest() override
    {
        beginTest ("key parsing");
        expectEquals (MusicalKey::fromString ("Am").toString(), juce::String ("Am"));
        expectEquals (MusicalKey::fromString ("F#min").toString(), juce::String ("F#m"));
        expectEquals (MusicalKey::fromString ("Bb").toString(), juce::String ("A#"));
        expectEquals (MusicalKey::fromString ("Ebm").toString(), juce::String ("D#m"));
        expectEquals (MusicalKey::fromString ("C major").toString(), juce::String ("C"));
        expectEquals (MusicalKey::fromString ("8A").toString(), juce::String ("Am"));
        expectEquals (MusicalKey::fromString ("8B").toString(), juce::String ("C"));
        expect (! MusicalKey::fromString ("Hello").isValid());

        beginTest ("camelot + int roundtrip");
        expectEquals (MusicalKey::fromString ("Am").toCamelot(), juce::String ("8A"));
        expectEquals (MusicalKey::fromString ("G").toCamelot(), juce::String ("9B"));
        for (auto k : theory::allKeys())
            expect (MusicalKey::fromInt (k.toInt()) == k);

        beginTest ("transpose distance");
        expectEquals (theory::semitonesBetween (MusicalKey::fromString ("C"), MusicalKey::fromString ("D")), 2);
        expectEquals (theory::semitonesBetween (MusicalKey::fromString ("C"), MusicalKey::fromString ("A")), -3);
        expectEquals (theory::semitonesBetween (MusicalKey::fromString ("Am"), MusicalKey::fromString ("C")), 0); // relative keys
        expectEquals (theory::semitonesBetween (MusicalKey::fromString ("Am"), MusicalKey::fromString ("Bm")), 2);
    }
};

struct FilenameTests : juce::UnitTest
{
    FilenameTests() : juce::UnitTest ("Filename hints", "SampleManager") {}
    void runTest() override
    {
        beginTest ("bpm");
        expectEquals (filename::parse ("Loop_128_Am_Bass").bpm, 128.0);
        expectEquals (filename::parse ("Dark Pad 90bpm").bpm, 90.0);
        expectEquals (filename::parse ("BPM140 Trap Hat Loop").bpm, 140.0);
        expectEquals (filename::parse ("Kick_01").bpm, 0.0);
        expectEquals (filename::parse ("Snare 808 hard").bpm, 0.0); // 808 is not a tempo

        beginTest ("key");
        expectEquals (filename::parse ("Loop_128_Am_Bass").key.toString(), juce::String ("Am"));
        expectEquals (filename::parse ("Synth Stab (F#)").key.toString(), juce::String ("F#"));
        expectEquals (filename::parse ("Chords_C_minor_120").key.toString(), juce::String ("Cm"));
        expectEquals (filename::parse ("Piano Ebmaj 100").key.toString(), juce::String ("D#"));
        expect (! filename::parse ("Bass_Deep_Face").key.isValid());
        expect (! filename::parse ("Snare_A").key.isValid());

        beginTest ("type");
        expectEquals (filename::parse ("Drum Loop 01").typeHint, 1);
        expectEquals (filename::parse ("Kick One Shot").typeHint, 2);
        expectEquals (filename::parse ("Kick", "One Shots Drums").typeHint, 2);

        beginTest ("tokenize");
        expect (filename::tokenize ("HiHatOpen_02") == juce::StringArray ({ "hi", "hat", "open", "02" }));
        expect (filename::tokenize ("Kick808Sub") == juce::StringArray ({ "kick", "808", "sub" }));
    }
};

struct AnalyzerTests : juce::UnitTest
{
    AnalyzerTests() : juce::UnitTest ("Audio analyzer", "SampleManager") {}
    void runTest() override
    {
        const double sr = 44100.0;

        beginTest ("key: C major progression");
        {
            // C - F - G - C (I IV V I)
            auto b = test::chordProgression ({ { 48, 60, 64, 67 }, { 53, 65, 69, 72 }, { 55, 67, 71, 74 }, { 48, 60, 64, 67 } }, 1.0, sr);
            auto r = AudioAnalyzer::analyse (b, sr, {});
            expectEquals (r.key.toString(), juce::String ("C"));
        }
        beginTest ("key: A minor progression");
        {
            // Am - Dm - E - Am
            auto b = test::chordProgression ({ { 45, 57, 60, 64 }, { 50, 62, 65, 69 }, { 52, 64, 68, 71 }, { 45, 57, 60, 64 } }, 1.0, sr);
            auto r = AudioAnalyzer::analyse (b, sr, {});
            expectEquals (r.key.toString(), juce::String ("Am"));
        }
        beginTest ("key: single note one-shot");
        {
            auto b = test::pluck (62, 0.8, sr); // D4
            auto r = AudioAnalyzer::analyse (b, sr, {});
            expect (! r.isLoop);
            expectEquals (r.key.toString(), juce::String ("D"));
            expect (r.key.mode == MusicalKey::Note);
        }
        beginTest ("tempo + loop detection");
        for (double bpm : { 90.0, 124.0, 140.0, 174.0 })
        {
            auto b = test::drumLoop (bpm, 2, sr);
            auto r = AudioAnalyzer::analyse (b, sr, {});
            expect (r.isLoop, "loop not detected at " + juce::String (bpm));
            // half/double tempo is musically equivalent but we want the exact value here
            expectWithinAbsoluteError (r.bpm, bpm, 0.5);
        }
        beginTest ("4-bar loop with a sustained bass");
        {
            auto b = test::drumLoop (132.0, 4, sr);
            auto bass = test::pluck (38, b.getNumSamples() / sr, sr);
            for (int i = 0; i < b.getNumSamples(); ++i) { b.addSample (0, i, 0.25f * bass.getSample (0, i) / 0.6f); b.addSample (1, i, 0.25f * bass.getSample (0, i) / 0.6f); }
            double er = 0, gs = 0;
            auto on = AudioAnalyzer::onsetEnvelope (b.getReadPointer (0), b.getNumSamples(), sr, er);
            const double gb = AudioAnalyzer::tempoFromBeatGrid (on, er, b.getNumSamples() / sr, &gs);
            auto r = AudioAnalyzer::analyse (b, sr, {});
            expectWithinAbsoluteError (r.bpm, 132.0, 0.5, "grid " + juce::String (gb) + " score " + juce::String (gs, 3));
        }
        beginTest ("filename hints override detection");
        {
            auto b = test::drumLoop (120.0, 1, sr);
            FilenameHints h; h.bpm = 120.0; h.key = MusicalKey::fromString ("Gm");
            auto r = AudioAnalyzer::analyse (b, sr, h);
            expectEquals (r.bpm, 120.0);
            expectEquals (r.key.toString(), juce::String ("Gm"));
        }
        beginTest ("noise burst is an unpitched one-shot");
        {
            auto b = test::noiseBurst (0.3, sr);
            auto r = AudioAnalyzer::analyse (b, sr, {});
            expect (! r.isLoop);
            expect (r.bpm == 0.0);
            expect (! r.key.isValid());
            expect (r.effectiveSeconds < r.durationSeconds);
        }
        beginTest ("loop length tempo");
        expectWithinAbsoluteError (AudioAnalyzer::tempoFromLoopLength (4.0, 118.0), 120.0, 0.001);
        expectWithinAbsoluteError (AudioAnalyzer::tempoFromLoopLength (8.0, 62.0), 120.0, 0.001); // half-time guess
        expectEquals (AudioAnalyzer::tempoFromLoopLength (4.0, 0.0), 0.0);

        beginTest ("fingerprint");
        {
            auto a = test::drumLoop (128.0, 1, sr);
            auto b2 = a; b2.applyGain (0.5f); // same audio, quieter
            auto c = test::drumLoop (100.0, 1, sr);
            auto fa = AudioAnalyzer::analyse (a, sr).fingerprint;
            expectEquals (AudioAnalyzer::analyse (b2, sr).fingerprint, fa);
            expect (AudioAnalyzer::analyse (c, sr).fingerprint != fa);
        }
        beginTest ("overview + zero crossing");
        {
            auto b = test::pluck (60, 0.5, sr);
            auto w = AudioAnalyzer::makeOverview (b, sr);
            expect (! w.isEmpty());
            const int z = AudioAnalyzer::nearestZeroCrossing (b.getReadPointer (0), b.getNumSamples(), 1000);
            expect (std::abs (z - 1000) < 200);
            auto* d = b.getReadPointer (0);
            expect ((d[z - 1] <= 0 && d[z] >= 0) || (d[z - 1] >= 0 && d[z] <= 0));
        }
    }
};

static MusicTheoryTests musicTheoryTests;
static FilenameTests filenameTests;
static AnalyzerTests analyzerTests;
