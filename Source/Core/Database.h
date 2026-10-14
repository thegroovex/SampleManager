#pragma once
#include <juce_core/juce_core.h>
#include "SearchQuery.h"
#include "AudioAnalyzer.h"
#include "AutoTagger.h"
#include <optional>
#include <set>

struct sqlite3;

namespace sm
{
struct LibraryRow   { juce::int64 id = 0; juce::String path, name; int sampleCount = 0; };
struct TagRow       { juce::int64 id = 0; juce::String name; TagType type = TagType::Custom; int count = 0; };
struct CollectionRow{ juce::int64 id = 0; juce::String name; int count = 0; };
struct SavedSearchRow { juce::int64 id = 0; juce::String name, json; };

struct SampleRow
{
    juce::int64 id = 0, libraryId = 0;
    juce::String path, filename, libraryName, codec, tags;
    juce::int64 fileSize = 0, addedAt = 0; // addedAt: ms since epoch
    double duration = 0, effectiveLength = 0, sampleRate = 0, bpm = 0;
    int bitDepth = 0, channels = 0;
    MusicalKey key;
    bool isLoop = false, favourite = false, analysed = false, hasDuplicate = false;
    juce::String fingerprint;

    juce::File file() const { return juce::File (path); }
};

/** SQLite library shared by the standalone app and all plugin instances.
    WAL mode allows several processes to read while one writes; every public
    method is serialised with a lock so one object can be shared between threads. */
class Database
{
public:
    explicit Database (const juce::File& file);
    ~Database();

    bool isOpen() const noexcept { return db != nullptr; }
    juce::String getLastError() const { return lastError; }
    juce::File getFile() const { return dbFile; }

    /** Increments whenever ANOTHER connection (other thread/process) committed changes. */
    juce::int64 dataVersion();
    /** Local change counter: bumped by writes through this object. */
    juce::int64 localVersion() const noexcept { return localChanges.load(); }

    // Libraries -----------------------------------------------------------
    juce::int64 addLibrary (const juce::File& folder);
    void removeLibrary (juce::int64 id);
    std::vector<LibraryRow> libraries();

    // Scanning --------------------------------------------------------------
    /** Inserts a file or flags it for re-analysis when size/mtime changed. Returns the sample id. */
    juce::int64 upsertSampleFile (juce::int64 libraryId, const juce::File& file, juce::int64 size, juce::int64 modTime);
    /** Removes samples of a library whose path is not in 'seen'. Returns number removed. */
    int removeMissing (juce::int64 libraryId, const std::set<juce::String>& seen);
    std::vector<std::pair<juce::int64, juce::String>> samplesNeedingAnalysis (int limit);
    void storeAnalysis (juce::int64 sampleId, const AnalysisResult& r, const juce::Array<TagSuggestion>& autoTags);
    void markAnalysisFailed (juce::int64 sampleId);
    void requestReanalysis (const juce::Array<juce::int64>& ids);

    // Querying --------------------------------------------------------------
    std::optional<SampleRow> sample (juce::int64 id);
    std::vector<SampleRow> search (const SearchQuery& q);
    juce::int64 randomSample (const SearchQuery& q, juce::int64 excludeId = 0);
    std::vector<std::vector<juce::int64>> duplicateGroups();
    WaveformOverview overview (juce::int64 sampleId);
    int sampleCount();
    int pendingAnalysisCount();

    // Editing ---------------------------------------------------------------
    void setFavourite (const juce::Array<juce::int64>& ids, bool fav);
    void setBpm (const juce::Array<juce::int64>& ids, double bpm);
    void setKey (const juce::Array<juce::int64>& ids, MusicalKey key);
    void setLoop (const juce::Array<juce::int64>& ids, bool isLoop);

    // Tags ------------------------------------------------------------------
    juce::int64 tagId (const juce::String& name, TagType type, bool create = true);
    std::vector<TagRow> tags (bool onlyUsed = true);
    std::vector<TagRow> tagsFor (juce::int64 sampleId);
    void addTag (const juce::Array<juce::int64>& sampleIds, juce::int64 tagId);
    void removeTag (const juce::Array<juce::int64>& sampleIds, juce::int64 tagId);
    void deleteTag (juce::int64 tagId);
    void renameTag (juce::int64 tagId, const juce::String& newName);

    // Collections -----------------------------------------------------------
    juce::int64 createCollection (const juce::String& name);
    void renameCollection (juce::int64 id, const juce::String& name);
    void deleteCollection (juce::int64 id);
    std::vector<CollectionRow> collections();
    void addToCollection (juce::int64 collectionId, const juce::Array<juce::int64>& ids);
    void removeFromCollection (juce::int64 collectionId, const juce::Array<juce::int64>& ids);

    // Saved searches --------------------------------------------------------
    juce::int64 saveSearch (const juce::String& name, const SearchQuery& q);
    void deleteSavedSearch (juce::int64 id);
    std::vector<SavedSearchRow> savedSearches();

    // Settings (shared between all instances) -------------------------------
    juce::String getSetting (const juce::String& key, const juce::String& fallback = {});
    void setSetting (const juce::String& key, const juce::String& value);

    struct Transaction
    {
        explicit Transaction (Database& d);
        ~Transaction();
        void commit();
        Database& db; bool done = false;
    };

private:
    class Statement;
    friend class Statement;

    bool exec (const char* sql);
    void migrate();
    void touched() noexcept { ++localChanges; }
    juce::String buildWhere (const SearchQuery& q, juce::Array<juce::var>& binds) const;
    std::vector<SampleRow> runSampleQuery (const juce::String& sql, const juce::Array<juce::var>& binds);

    sqlite3* db = nullptr;
    juce::File dbFile;
    juce::String lastError;
    juce::CriticalSection lock;
    std::atomic<juce::int64> localChanges { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Database)
};
}
