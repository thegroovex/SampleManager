#include <juce_core/juce_core.h>
#include "Core/Database.h"
#include "Core/AutoTagger.h"

using namespace sm;

struct DatabaseTests : juce::UnitTest
{
    DatabaseTests() : juce::UnitTest ("Database", "SampleManager") {}

    AnalysisResult makeResult (double bpm, const char* key, bool loop, const char* fp)
    {
        AnalysisResult r;
        r.durationSeconds = loop ? 4.0 : 0.5; r.effectiveSeconds = r.durationSeconds;
        r.sampleRate = 44100; r.bitDepth = 24; r.channels = 2; r.codec = "WAV";
        r.bpm = bpm; r.key = MusicalKey::fromString (key); r.isLoop = loop; r.fingerprint = fp;
        return r;
    }

    void runTest() override
    {
        auto dir = juce::File::createTempFile ("smdb");
        dir.createDirectory();
        auto dbFile = dir.getChildFile ("library.sqlite");
        auto libDir = dir.getChildFile ("Samples");

        Database db (dbFile);
        beginTest ("open + migrate");
        expect (db.isOpen(), db.getLastError());

        auto lib = db.addLibrary (libDir);
        expect (lib > 0);
        expectEquals (db.addLibrary (libDir), lib); // idempotent

        auto f1 = libDir.getChildFile ("House Pack/Loops/Bass_Loop_124_Am.wav");
        auto f2 = libDir.getChildFile ("House Pack/Kicks/Kick_01.wav");
        auto f3 = libDir.getChildFile ("Techno Tools/Loops/Top_Loop_130.wav");
        auto f4 = libDir.getChildFile ("Techno Tools/Copies/Kick_01_copy.wav");

        beginTest ("upsert + analysis + auto tags");
        auto id1 = db.upsertSampleFile (lib, f1, 1000, 1);
        auto id2 = db.upsertSampleFile (lib, f2, 2000, 1);
        auto id3 = db.upsertSampleFile (lib, f3, 3000, 1);
        auto id4 = db.upsertSampleFile (lib, f4, 2000, 1);
        expectEquals ((int) db.samplesNeedingAnalysis (100).size(), 4);
        db.storeAnalysis (id1, makeResult (124, "Am", true, "a"), AutoTagger::suggest (f1, libDir));
        db.storeAnalysis (id2, makeResult (0, "", false, "k"), AutoTagger::suggest (f2, libDir));
        db.storeAnalysis (id3, makeResult (130, "", true, "t"), AutoTagger::suggest (f3, libDir));
        db.storeAnalysis (id4, makeResult (0, "", false, "k"), AutoTagger::suggest (f4, libDir));
        expectEquals ((int) db.samplesNeedingAnalysis (100).size(), 0);
        expectEquals (db.sampleCount(), 4);

        // unchanged file keeps analysis, changed file is queued again
        db.upsertSampleFile (lib, f1, 1000, 1);
        expectEquals (db.pendingAnalysisCount(), 0);
        db.upsertSampleFile (lib, f1, 1001, 2);
        expectEquals (db.pendingAnalysisCount(), 1);
        db.storeAnalysis (id1, makeResult (124, "Am", true, "a"), AutoTagger::suggest (f1, libDir));

        beginTest ("search: text, type, bpm, key");
        SearchQuery q;
        expectEquals ((int) db.search (q).size(), 4);
        q.text = "kick";
        expectEquals ((int) db.search (q).size(), 2);
        q = {}; q.type = 1;
        expectEquals ((int) db.search (q).size(), 2);
        q = {}; q.bpmMin = 120; q.bpmMax = 126;
        auto rows = db.search (q);
        expectEquals ((int) rows.size(), 1);
        expectEquals (rows[0].id, id1);
        q = {}; q.keys.add (MusicalKey::fromString ("Am").toInt());
        expectEquals ((int) db.search (q).size(), 1);
        q = {}; q.text = "-kick";
        expectEquals ((int) db.search (q).size(), 2);

        beginTest ("search: tags AND / OR / exclude");
        auto kickTag = db.tagId ("Kick", TagType::Instrument, false);
        auto houseTag = db.tagId ("House", TagType::Genre, false);
        auto technoTag = db.tagId ("Techno", TagType::Genre, false);
        expect (kickTag > 0 && houseTag > 0 && technoTag > 0);
        q = {}; q.includeTags = { kickTag, houseTag }; q.tagsMatchAll = true;
        expectEquals ((int) db.search (q).size(), 1);
        q.tagsMatchAll = false;
        expectEquals ((int) db.search (q).size(), 3); // kick(2) + house(1 more)
        q = {}; q.excludeTags = { kickTag };
        expectEquals ((int) db.search (q).size(), 2);

        beginTest ("manual edits survive re-analysis");
        db.setBpm ({ id3 }, 129.5);
        db.storeAnalysis (id3, makeResult (130, "", true, "t"), AutoTagger::suggest (f3, libDir));
        expectEquals (db.sample (id3)->bpm, 129.5);
        auto custom = db.tagId ("Dark", TagType::Custom);
        db.addTag ({ id3 }, custom);
        db.storeAnalysis (id3, makeResult (130, "", true, "t"), AutoTagger::suggest (f3, libDir));
        bool found = false;
        for (auto& t : db.tagsFor (id3)) found |= t.id == custom;
        expect (found);

        beginTest ("duplicates");
        auto groups = db.duplicateGroups();
        expectEquals ((int) groups.size(), 1);
        expectEquals ((int) groups[0].size(), 2);
        q = {}; q.hideDuplicates = true;
        expectEquals ((int) db.search (q).size(), 3);
        expect (db.sample (id2)->hasDuplicate);

        beginTest ("similar");
        q = {}; q.similarTo = id2;
        rows = db.search (q);
        expect (! rows.empty());
        expectEquals (rows[0].id, id4); // the other kick ranks first

        beginTest ("favourites + collections + saved searches");
        db.setFavourite ({ id1, id3 }, true);
        q = {}; q.favouritesOnly = true;
        expectEquals ((int) db.search (q).size(), 2);
        auto col = db.createCollection ("Ideas");
        db.addToCollection (col, { id2, id3 });
        q = {}; q.collectionId = col;
        expectEquals ((int) db.search (q).size(), 2);
        expectEquals (db.collections()[0].count, 2);
        q = {}; q.type = 1; q.bpmMin = 120;
        auto ss = db.saveSearch ("Loops 120+", q);
        expect (ss > 0);
        auto restored = SearchQuery::fromJson (db.savedSearches()[0].json);
        expectEquals (restored.type, 1);
        expectEquals (restored.bpmMin, 120.0);
        expectEquals (restored.title, juce::String ("Loops 120+"));

        beginTest ("random");
        q = {};
        auto r = db.randomSample (q, id1);
        expect (r != 0 && r != id1);

        beginTest ("sorting");
        q = {}; q.sortColumn = "bpm"; q.ascending = true;
        rows = db.search (q);
        expectEquals (rows[0].id, id1); // 124 before 129.5, one-shots (0) last
        q.ascending = false; q.sortColumn = "name";
        rows = db.search (q);
        expectEquals (rows[0].filename, juce::String ("Top_Loop_130"));

        beginTest ("settings shared between connections + change detection");
        Database other (dbFile);
        expect (other.isOpen());
        const auto v0 = db.dataVersion();
        other.setSetting ("language", "de");
        expectEquals (db.getSetting ("language"), juce::String ("de"));
        expect (db.dataVersion() != v0);

        beginTest ("remove missing + remove library");
        std::set<juce::String> seen { f1.getFullPathName(), f2.getFullPathName() };
        expectEquals (db.removeMissing (lib, seen), 2);
        expectEquals (db.sampleCount(), 2);
        db.removeLibrary (lib);
        expectEquals (db.sampleCount(), 0);

        dir.deleteRecursively();
    }
};

static DatabaseTests databaseTests;
