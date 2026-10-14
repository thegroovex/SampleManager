#include "AutoTagger.h"
#include "FilenameParser.h"

namespace sm
{
juce::String tagTypeName (TagType t)
{
    switch (t)
    {
        case TagType::Instrument: return "Type";
        case TagType::Genre:      return "Genre";
        case TagType::Label:      return "Label";
        case TagType::Custom:     return "Custom";
    }
    return {};
}

namespace
{
    struct Rule { const char* tag; std::vector<const char*> patterns; };

    // Patterns are token sequences separated by spaces. A pattern matches when its tokens
    // appear consecutively in the tokenised path (file name weighs more than folders).
    const std::vector<Rule>& instrumentRules()
    {
        static const std::vector<Rule> rules {
            { "Kick",      { "kick", "kicks", "kck", "bd", "bassdrum", "bass drum", "kik" } },
            { "Snare",     { "snare", "snares", "snr", "sd", "rim", "rimshot" } },
            { "Clap",      { "clap", "claps", "clp" } },
            { "Hi-Hat",    { "hihat", "hi hat", "hi hats", "hihats", "hh", "hat", "hats", "closed hat", "open hat", "ohh", "chh" } },
            { "Cymbal",    { "cymbal", "cymbals", "crash", "ride", "china", "splash" } },
            { "Tom",       { "tom", "toms" } },
            { "Percussion",{ "perc", "percs", "percussion", "conga", "bongo", "tabla", "djembe", "cowbell", "shaker", "tambourine", "tamb", "clave", "woodblock", "triangle", "timbale" } },
            { "Drum Loop", { "drum loop", "drum loops", "drumloop", "beat loop", "top loop", "top loops", "toploop", "full drum", "breakbeat", "break", "breaks" } },
            { "808",       { "808", "808s" } },
            { "Bass",      { "bass", "bassline", "basses", "sub", "reese", "subbass", "sub bass" } },
            { "Synth",     { "synth", "synths", "saw", "supersaw", "analog", "analogue" } },
            { "Lead",      { "lead", "leads", "ld" } },
            { "Pad",       { "pad", "pads" } },
            { "Pluck",     { "pluck", "plucks" } },
            { "Chord",     { "chord", "chords", "stab", "stabs" } },
            { "Arp",       { "arp", "arps", "arpeggio" } },
            { "Keys",      { "keys", "key loop", "rhodes", "organ", "epiano", "wurli", "clav", "clavinet" } },
            { "Piano",     { "piano", "pianos" } },
            { "Guitar",    { "guitar", "guitars", "gtr", "acoustic guitar", "electric guitar" } },
            { "Strings",   { "string", "strings", "violin", "viola", "cello", "orchestra", "orchestral" } },
            { "Brass",     { "brass", "trumpet", "horn", "horns", "trombone", "sax", "saxophone" } },
            { "Woodwind",  { "flute", "flutes", "clarinet", "oboe", "woodwind" } },
            { "Bell",      { "bell", "bells", "chime", "chimes", "glock", "glockenspiel", "mallet", "marimba", "vibraphone", "kalimba" } },
            { "Vocal",     { "vocal", "vocals", "vox", "voice", "acapella", "acappella", "chant", "choir", "adlib", "adlibs", "phrase" } },
            { "FX",        { "fx", "sfx", "effect", "effects", "noise", "glitch", "foley" } },
            { "Riser",     { "riser", "risers", "uplifter", "build", "buildup", "rise", "sweep up", "upsweep" } },
            { "Downlifter",{ "downlifter", "downsweep", "fall", "sweep down" } },
            { "Impact",    { "impact", "impacts", "boom", "slam", "thud" } },
            { "Sweep",     { "sweep", "sweeps", "whoosh", "swoosh" } },
            { "Atmosphere",{ "atmos", "atmosphere", "ambience", "ambient pad", "texture", "textures", "drone", "soundscape" } },
            { "Fill",      { "fill", "fills", "roll", "rolls" } },
            { "Snap",      { "snap", "snaps", "finger snap" } },
        };
        return rules;
    }

    const std::vector<Rule>& genreRules()
    {
        static const std::vector<Rule> rules {
            { "House",        { "house" } },
            { "Deep House",   { "deep house", "deephouse" } },
            { "Tech House",   { "tech house", "techhouse" } },
            { "Techno",       { "techno" } },
            { "Minimal",      { "minimal" } },
            { "Trance",       { "trance", "psytrance", "psy" } },
            { "Drum & Bass",  { "dnb", "drum and bass", "drum n bass", "drumnbass", "jungle", "liquid" } },
            { "Dubstep",      { "dubstep", "riddim", "brostep" } },
            { "Trap",         { "trap" } },
            { "Hip Hop",      { "hip hop", "hiphop", "boom bap", "boombap", "rap" } },
            { "Drill",        { "drill" } },
            { "Lo-Fi",        { "lofi", "lo fi" } },
            { "R&B",          { "rnb", "r b", "r&b", "soul", "neo soul" } },
            { "Pop",          { "pop", "kpop", "k pop", "synthpop" } },
            { "EDM",          { "edm", "big room", "bigroom", "electro house", "festival" } },
            { "Future Bass",  { "future bass", "futurebass" } },
            { "Garage",       { "garage", "ukg", "2step", "2 step" } },
            { "Afro",         { "afro", "afrobeat", "afrobeats", "afro house", "amapiano" } },
            { "Latin",        { "reggaeton", "latin", "dembow", "baile", "moombahton" } },
            { "Ambient",      { "ambient", "chillout", "chill out", "downtempo" } },
            { "Cinematic",    { "cinematic", "trailer", "film", "score" } },
            { "Disco",        { "disco", "nu disco", "nudisco", "funk", "funky" } },
            { "Phonk",        { "phonk" } },
            { "Breaks",       { "breakbeat", "breaks" } },
            { "Hardstyle",    { "hardstyle", "hardcore", "gabber" } },
            { "Synthwave",    { "synthwave", "retrowave", "outrun" } },
            { "Jazz",         { "jazz", "jazzy" } },
            { "Rock",         { "rock", "indie" } },
        };
        return rules;
    }

    bool containsSequence (const juce::StringArray& tokens, const juce::StringArray& seq)
    {
        if (seq.isEmpty() || seq.size() > tokens.size()) return false;
        for (int i = 0; i + seq.size() <= tokens.size(); ++i)
        {
            bool ok = true;
            for (int j = 0; j < seq.size() && ok; ++j)
                ok = tokens[i + j] == seq[j];
            if (ok) return true;
        }
        // also match glued forms, e.g. "hihat" pattern vs tokens "hi" "hat"
        return false;
    }

    void applyRules (const std::vector<Rule>& rules, const juce::StringArray& tokens, TagType type,
                     juce::Array<TagSuggestion>& out)
    {
        for (auto& r : rules)
            for (auto* p : r.patterns)
            {
                auto seq = juce::StringArray::fromTokens (p, " ", "");
                if (containsSequence (tokens, seq))
                {
                    out.addIfNotAlreadyThere ({ r.tag, type });
                    break;
                }
            }
    }
}

const juce::StringArray& AutoTagger::instrumentTags()
{
    static juce::StringArray s = [] { juce::StringArray a; for (auto& r : instrumentRules()) a.add (r.tag); return a; }();
    return s;
}

const juce::StringArray& AutoTagger::genreTags()
{
    static juce::StringArray s = [] { juce::StringArray a; for (auto& r : genreRules()) a.add (r.tag); return a; }();
    return s;
}

juce::String AutoTagger::labelFromPath (const juce::File& file, const juce::File& libraryRoot)
{
    auto rel = file.getRelativePathFrom (libraryRoot).replaceCharacter ('\\', '/');
    auto parts = juce::StringArray::fromTokens (rel, "/", "");
    parts.removeEmptyStrings();
    if (parts.size() <= 1) return libraryRoot.getFileName(); // files directly in the root

    // Splice: .../Splice/Samples/packs/<Pack>/...
    for (int i = 0; i + 1 < parts.size() - 1; ++i)
        if (parts[i].equalsIgnoreCase ("packs")) return parts[i + 1];
    // Loopcloud / Noiiz keep the pack below "Samples" or "Library"
    for (int i = 0; i + 1 < parts.size() - 1; ++i)
        if (parts[i].equalsIgnoreCase ("samples") || parts[i].equalsIgnoreCase ("library") || parts[i].equalsIgnoreCase ("downloads"))
            return parts[i + 1];
    return parts[0];
}

juce::Array<TagSuggestion> AutoTagger::suggest (const juce::File& file, const juce::File& libraryRoot, const AnalysisResult* a)
{
    juce::Array<TagSuggestion> out;

    auto nameTokens = filename::tokenize (file.getFileNameWithoutExtension());
    auto rel = file.getParentDirectory().getRelativePathFrom (libraryRoot);
    if (file.getParentDirectory() == libraryRoot) rel = {};
    auto folderTokens = filename::tokenize (rel);

    // the file name decides the sound type; only fall back to folders when the name says nothing
    applyRules (instrumentRules(), nameTokens, TagType::Instrument, out);
    if (out.isEmpty())
        applyRules (instrumentRules(), folderTokens, TagType::Instrument, out);

    // "Drum Loop" implies no single drum type unless named
    juce::StringArray all (nameTokens); all.addArray (folderTokens);
    applyRules (genreRules(), all, TagType::Genre, out);

    // "Deep House" also matched "House" - keep the more specific one only
    auto has = [&] (const char* n) { return out.contains ({ n, TagType::Genre }); };
    if (has ("Deep House") || has ("Tech House") || has ("Afro"))
        out.removeFirstMatchingValue ({ "House", TagType::Genre });

    auto label = labelFromPath (file, libraryRoot);
    if (label.isNotEmpty())
        out.add ({ label, TagType::Label });

    // audio based fallback when the name gives no sound type
    bool hasType = false;
    for (auto& t : out) hasType |= t.type == TagType::Instrument;
    if (! hasType && a != nullptr)
    {
        if (! a->isLoop && a->durationSeconds < 1.5)
        {
            if (a->lowRatio > 0.55f && a->spectralCentroid < 400.0f && a->attackSeconds < 0.03f)
                out.add ({ "Kick", TagType::Instrument });
            else if (a->spectralCentroid > 6000.0f && a->effectiveSeconds < 0.6)
                out.add ({ "Hi-Hat", TagType::Instrument });
            else if (a->spectralCentroid > 1500.0f && a->attackSeconds < 0.02f && ! a->key.isValid())
                out.add ({ "Percussion", TagType::Instrument });
        }
        else if (a->isLoop && a->lowRatio > 0.5f && a->key.isValid())
            out.add ({ "Bass", TagType::Instrument });
    }
    return out;
}
}
