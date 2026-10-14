#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include "Database.h"

namespace sm
{
/** Background worker: walks every library folder, keeps the database in sync
    with the files on disk and analyses new/changed samples (key, BPM, loop,
    tags, waveform, fingerprint). Uses its own database connection.
    An inter-process lock makes sure only one SampleManager (app or plugin)
    scans at a time; the others simply see the results appear. */
class LibraryScanner : private juce::Thread
{
public:
    explicit LibraryScanner (const juce::File& databaseFile);
    ~LibraryScanner() override;

    /** Scan all libraries (or just one) and analyse pending samples. */
    void requestScan (juce::int64 libraryId = 0);
    /** Only analyse pending samples (after a re-analyse request). */
    void requestAnalysis();
    void stop();

    struct Progress
    {
        bool running = false;
        int filesFound = 0;
        int analysed = 0;
        int toAnalyse = 0;
        juce::String currentFile;
    };
    Progress getProgress() const;

    /** Increments whenever a batch of changes was committed (poll from a timer). */
    int getBatchCounter() const noexcept { return batchCounter.load(); }

private:
    void run() override;
    void scanLibrary (Database& db, const LibraryRow& lib);
    void analysePending (Database& db);
    void notifyBatch();

    juce::File dbFile;
    juce::AudioFormatManager formats;
    juce::InterProcessLock ipLock { "SampleManagerScanner" };

    mutable juce::CriticalSection progressLock;
    Progress progress;
    std::atomic<bool> scanRequested { false }, analysisRequested { false };
    std::atomic<juce::int64> requestedLibrary { 0 };
    std::atomic<int> batchCounter { 0 };
    juce::WaitableEvent wake;
};
}
