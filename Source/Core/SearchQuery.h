#pragma once
#include <juce_core/juce_core.h>

namespace sm
{
/** Everything that defines one search tab. Serialisable for saved searches and tab restore. */
struct SearchQuery
{
    juce::String title;                   // tab title
    juce::String text;                    // free text, words AND-ed, matched against name, path and tags
    juce::Array<juce::int64> includeTags;
    juce::Array<juce::int64> excludeTags;
    bool   tagsMatchAll = true;           // AND (true) or OR (false) for includeTags
    int    type = 0;                      // 0 all, 1 loops, 2 one-shots
    double bpmMin = 0, bpmMax = 0;        // 0 = open
    juce::Array<int> keys;                // MusicalKey::toInt() values
    juce::int64 collectionId = 0;
    juce::int64 libraryId = 0;
    juce::int64 similarTo = 0;            // sample id: rank by shared tags
    bool   favouritesOnly = false;
    bool   hideDuplicates = false;
    juce::String sortColumn = "name";
    bool   ascending = true;
    int    limit = 20000;

    juce::var toVar() const;
    static SearchQuery fromVar (const juce::var& v);
    juce::String toJson() const { return juce::JSON::toString (toVar(), true); }
    static SearchQuery fromJson (const juce::String& json) { return fromVar (juce::JSON::parse (json)); }

    bool isEmptyFilter() const;
    juce::String describe() const; // short automatic tab title
};
}
