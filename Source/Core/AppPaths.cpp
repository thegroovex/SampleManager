#include "AppPaths.h"

namespace sm::paths
{
juce::File dataDirectory()
{
    auto env = juce::SystemStats::getEnvironmentVariable ("SAMPLEMANAGER_DATA_DIR", {});
    juce::File dir = env.isNotEmpty()
        ? juce::File (env)
        : juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
         #if JUCE_MAC
            .getChildFile ("Application Support")
         #endif
            .getChildFile ("SampleManager");
    dir.createDirectory();
    return dir;
}

juce::File databaseFile() { return dataDirectory().getChildFile ("library.sqlite"); }

juce::File renderDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("SampleManagerRenders");
    dir.createDirectory();
    return dir;
}

const juce::StringArray& audioExtensions()
{
    static const juce::StringArray exts { ".wav", ".wave", ".aif", ".aiff", ".flac", ".mp3", ".ogg" };
    return exts;
}

bool isAudioFile (const juce::File& f)
{
    return audioExtensions().contains (f.getFileExtension().toLowerCase())
        && ! f.getFileName().startsWithChar ('.');
}
}
