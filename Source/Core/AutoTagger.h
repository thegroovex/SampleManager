#pragma once
#include <juce_core/juce_core.h>
#include "AudioAnalyzer.h"

namespace sm
{
enum class TagType { Instrument = 0, Genre = 1, Label = 2, Custom = 3 };

struct TagSuggestion
{
    juce::String name;
    TagType type;
    bool operator== (const TagSuggestion& o) const { return name == o.name && type == o.type; }
};

juce::String tagTypeName (TagType t);

/** Derives tags from the path (sound type, genre, label/pack) and from
    audio features when the name gives nothing away. Fully offline. */
class AutoTagger
{
public:
    /** libraryRoot: the folder the user added. The first folder below it is used as label/pack name,
        with special handling for Splice / Loopcloud / Noiiz folder layouts. */
    static juce::Array<TagSuggestion> suggest (const juce::File& file, const juce::File& libraryRoot,
                                               const AnalysisResult* analysis = nullptr);

    static juce::String labelFromPath (const juce::File& file, const juce::File& libraryRoot);
    static const juce::StringArray& instrumentTags();
    static const juce::StringArray& genreTags();
};
}
