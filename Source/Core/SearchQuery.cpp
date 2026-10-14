#include "SearchQuery.h"
#include "MusicTheory.h"

namespace sm
{
static juce::var toVarArray (const juce::Array<juce::int64>& a)
{
    juce::Array<juce::var> v;
    for (auto x : a) v.add (juce::var (x));
    return v;
}

juce::var SearchQuery::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("title", title);
    o->setProperty ("text", text);
    o->setProperty ("include", toVarArray (includeTags));
    o->setProperty ("exclude", toVarArray (excludeTags));
    o->setProperty ("all", tagsMatchAll);
    o->setProperty ("type", type);
    o->setProperty ("bpmMin", bpmMin);
    o->setProperty ("bpmMax", bpmMax);
    juce::Array<juce::var> k; for (auto x : keys) k.add (x);
    o->setProperty ("keys", k);
    o->setProperty ("collection", collectionId);
    o->setProperty ("library", libraryId);
    o->setProperty ("similar", similarTo);
    o->setProperty ("fav", favouritesOnly);
    o->setProperty ("hideDup", hideDuplicates);
    o->setProperty ("sort", sortColumn);
    o->setProperty ("asc", ascending);
    return juce::var (o);
}

SearchQuery SearchQuery::fromVar (const juce::var& v)
{
    SearchQuery q;
    if (! v.isObject()) return q;
    q.title = v["title"].toString();
    q.text = v["text"].toString();
    if (auto* a = v["include"].getArray()) for (auto& x : *a) q.includeTags.add ((juce::int64) x);
    if (auto* a = v["exclude"].getArray()) for (auto& x : *a) q.excludeTags.add ((juce::int64) x);
    q.tagsMatchAll = v.getProperty ("all", true);
    q.type = v["type"];
    q.bpmMin = v["bpmMin"];
    q.bpmMax = v["bpmMax"];
    if (auto* a = v["keys"].getArray()) for (auto& x : *a) q.keys.add ((int) x);
    q.collectionId = (juce::int64) v["collection"];
    q.libraryId = (juce::int64) v["library"];
    q.similarTo = (juce::int64) v["similar"];
    q.favouritesOnly = v["fav"];
    q.hideDuplicates = v["hideDup"];
    q.sortColumn = v.getProperty ("sort", "name").toString();
    q.ascending = v.getProperty ("asc", true);
    return q;
}

bool SearchQuery::isEmptyFilter() const
{
    return text.isEmpty() && includeTags.isEmpty() && excludeTags.isEmpty() && type == 0
        && bpmMin <= 0 && bpmMax <= 0 && keys.isEmpty() && collectionId == 0 && libraryId == 0
        && similarTo == 0 && ! favouritesOnly;
}

juce::String SearchQuery::describe() const
{
    if (title.isNotEmpty()) return title;
    juce::StringArray parts;
    if (text.isNotEmpty()) parts.add (text);
    if (type == 1) parts.add ("Loops");
    if (type == 2) parts.add ("One-Shots");
    if (keys.size() == 1) parts.add (MusicalKey::fromInt (keys[0]).toString());
    if (bpmMin > 0 || bpmMax > 0) parts.add (juce::String ((int) bpmMin) + "-" + juce::String ((int) bpmMax) + " BPM");
    if (favouritesOnly) parts.add ("Favourites");
    return parts.isEmpty() ? juce::String ("All Samples") : parts.joinIntoString (" ");
}
}
