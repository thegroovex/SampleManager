#include <juce_core/juce_core.h>
#include "Core/AutoTagger.h"

using namespace sm;

struct TaggerTests : juce::UnitTest
{
    TaggerTests() : juce::UnitTest ("Auto tagger", "SampleManager") {}

    bool has (const juce::Array<TagSuggestion>& tags, const juce::String& name, TagType t)
    {
        return tags.contains ({ name, t });
    }

    void runTest() override
    {
        const auto base = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("smtest");
        const juce::File root = base.getChildFile ("Samples");

        beginTest ("sound type from file name");
        {
            auto t = AutoTagger::suggest (root.getChildFile ("Vendor Pack/Drums/Kick_Hard_01.wav"), root);
            expect (has (t, "Kick", TagType::Instrument));
            expect (! has (t, "Snare", TagType::Instrument));
            t = AutoTagger::suggest (root.getChildFile ("X/HiHatOpen_03.wav"), root);
            expect (has (t, "Hi-Hat", TagType::Instrument));
            t = AutoTagger::suggest (root.getChildFile ("X/Vox_Chop_Am.wav"), root);
            expect (has (t, "Vocal", TagType::Instrument));
        }
        beginTest ("folder fallback");
        {
            auto t = AutoTagger::suggest (root.getChildFile ("Pack/Snares/001.wav"), root);
            expect (has (t, "Snare", TagType::Instrument));
        }
        beginTest ("genre");
        {
            auto t = AutoTagger::suggest (root.getChildFile ("Deep House Essentials/Loops/Bass_124.wav"), root);
            expect (has (t, "Deep House", TagType::Genre));
            expect (! has (t, "House", TagType::Genre));
            expect (has (t, "Bass", TagType::Instrument));
            t = AutoTagger::suggest (root.getChildFile ("DnB Rollers/Break_174.wav"), root);
            expect (has (t, "Drum & Bass", TagType::Genre));
        }
        beginTest ("label / pack");
        {
            expectEquals (AutoTagger::labelFromPath (root.getChildFile ("Cymatics - Oracle/Kicks/K1.wav"), root), juce::String ("Cymatics - Oracle"));
            const juce::File splice = base.getChildFile ("Splice");
            expectEquals (AutoTagger::labelFromPath (splice.getChildFile ("Samples/packs/Lofi Dreams/drums/k.wav"), splice), juce::String ("Lofi Dreams"));
            expectEquals (AutoTagger::labelFromPath (root.getChildFile ("loose.wav"), root), juce::String ("Samples"));
        }
        beginTest ("audio fallback");
        {
            AnalysisResult a;
            a.durationSeconds = 0.4; a.effectiveSeconds = 0.4; a.isLoop = false;
            a.lowRatio = 0.8f; a.spectralCentroid = 120.0f; a.attackSeconds = 0.005f;
            auto t = AutoTagger::suggest (root.getChildFile ("Pack/sound_17.wav"), root, &a);
            expect (has (t, "Kick", TagType::Instrument));
        }
    }
};

static TaggerTests taggerTests;
