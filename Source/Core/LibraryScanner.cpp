#include "LibraryScanner.h"
#include "AppPaths.h"
#include "AutoTagger.h"

namespace sm
{
LibraryScanner::LibraryScanner (const juce::File& databaseFile)
    : juce::Thread ("SampleManager Scanner"), dbFile (databaseFile)
{
    formats.registerBasicFormats();
    startThread (juce::Thread::Priority::low);
}

LibraryScanner::~LibraryScanner()
{
    stop();
}

void LibraryScanner::stop()
{
    signalThreadShouldExit();
    wake.signal();
    stopThread (10000);
}

void LibraryScanner::requestScan (juce::int64 libraryId)
{
    requestedLibrary = libraryId;
    scanRequested = true;
    wake.signal();
}

void LibraryScanner::requestAnalysis()
{
    analysisRequested = true;
    wake.signal();
}

LibraryScanner::Progress LibraryScanner::getProgress() const
{
    const juce::ScopedLock sl (progressLock);
    return progress;
}

void LibraryScanner::notifyBatch()
{
    batchCounter++;
}

void LibraryScanner::run()
{
    while (! threadShouldExit())
    {
        wake.wait (2000);
        if (threadShouldExit()) break;
        if (! scanRequested && ! analysisRequested) continue;

        // only one process scans at a time; others retry later
        juce::InterProcessLock::ScopedLockType ipl (ipLock);
        if (! ipl.isLocked()) { juce::Thread::sleep (500); continue; }

        Database db (dbFile);
        if (! db.isOpen()) continue;

        {
            const juce::ScopedLock sl (progressLock);
            progress = {};
            progress.running = true;
        }

        if (scanRequested.exchange (false))
        {
            const auto only = requestedLibrary.load();
            for (auto& lib : db.libraries())
            {
                if (threadShouldExit()) break;
                if (only == 0 || lib.id == only)
                    scanLibrary (db, lib);
            }
            notifyBatch();
        }
        analysisRequested = false;
        analysePending (db);

        {
            const juce::ScopedLock sl (progressLock);
            progress.running = false;
            progress.currentFile.clear();
        }
        notifyBatch();
    }
}

void LibraryScanner::scanLibrary (Database& db, const LibraryRow& lib)
{
    juce::File root (lib.path);
    if (! root.isDirectory()) return; // e.g. unplugged drive: keep entries

    std::set<juce::String> seen;
    int sinceCommit = 0;
    auto tx = std::make_unique<Database::Transaction> (db);
    for (const auto& entry : juce::RangedDirectoryIterator (root, true, "*", juce::File::findFiles))
    {
        if (threadShouldExit()) break;
        auto f = entry.getFile();
        if (! paths::isAudioFile (f)) continue;
        seen.insert (f.getFullPathName());
        db.upsertSampleFile (lib.id, f, entry.getFileSize(), entry.getModificationTime().toMilliseconds());
        {
            const juce::ScopedLock sl (progressLock);
            progress.filesFound++;
            progress.currentFile = f.getFileName();
        }
        if (++sinceCommit >= 500)
        {
            tx->commit();
            tx = std::make_unique<Database::Transaction> (db);
            sinceCommit = 0;
            notifyBatch();
        }
    }
    tx->commit();
    tx.reset();
    if (! threadShouldExit())
        db.removeMissing (lib.id, seen);
}

void LibraryScanner::analysePending (Database& db)
{
    std::map<juce::int64, juce::File> libRoots;
    for (auto& l : db.libraries()) libRoots[l.id] = juce::File (l.path);

    {
        const juce::ScopedLock sl (progressLock);
        progress.toAnalyse = db.pendingAnalysisCount();
    }

    int sinceNotify = 0;
    while (! threadShouldExit())
    {
        auto batch = db.samplesNeedingAnalysis (64);
        if (batch.empty()) break;
        for (auto& [id, path] : batch)
        {
            if (threadShouldExit()) return;
            juce::File f (path);
            {
                const juce::ScopedLock sl (progressLock);
                progress.currentFile = f.getFileName();
            }
            AnalysisResult r;
            if (! f.existsAsFile() || ! AudioAnalyzer::analyseFile (formats, f, r))
            {
                db.markAnalysisFailed (id);
            }
            else
            {
                auto row = db.sample (id);
                juce::File root = row ? libRoots[row->libraryId] : f.getParentDirectory();
                db.storeAnalysis (id, r, AutoTagger::suggest (f, root, &r));
            }
            {
                const juce::ScopedLock sl (progressLock);
                progress.analysed++;
            }
            if (++sinceNotify >= 25) { sinceNotify = 0; notifyBatch(); }
        }
    }
}
}
