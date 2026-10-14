#pragma once
#include "Core/Database.h"
#include "Core/LibraryScanner.h"
#include "Core/AppPaths.h"

namespace sm
{
/** One per process (juce::SharedResourcePointer): the database connection used by
    the UI and the background scanner. Every plugin instance in a DAW shares it;
    the standalone app (another process) opens the same file. */
struct SharedLibrary
{
    SharedLibrary()
        : db (paths::databaseFile()),
          scanner (paths::databaseFile())
    {
        if (db.getSetting ("rescanOnStartup", "1") == "1")
            scanner.requestScan();
        else
            scanner.requestAnalysis();
    }

    Database db;
    LibraryScanner scanner;
};
}
