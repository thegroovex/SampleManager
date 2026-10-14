#pragma once
#include <juce_core/juce_core.h>

namespace sm::paths
{
    /** Shared data folder used by the standalone app AND every plugin instance,
        so all of them work on the same library database.
        macOS:   ~/Library/Application Support/SampleManager
        Windows: %APPDATA%\SampleManager
        Linux:   ~/.config/SampleManager
        Can be overridden with the environment variable SAMPLEMANAGER_DATA_DIR. */
    juce::File dataDirectory();
    juce::File databaseFile();
    /** Temp folder for processed renders used by drag & drop. */
    juce::File renderDirectory();
    /** Audio file extensions recognised by the scanner (lower case, with dot). */
    const juce::StringArray& audioExtensions();
    bool isAudioFile (const juce::File& f);
}
