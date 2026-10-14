#include "Database.h"
#include <sqlite3.h>
#include <map>

namespace sm
{
//==============================================================================
class Database::Statement
{
public:
    Statement (Database& d, const juce::String& sql) : owner (d)
    {
        if (sqlite3_prepare_v2 (d.db, sql.toRawUTF8(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            d.lastError = sqlite3_errmsg (d.db);
            DBG ("SQL error: " << d.lastError << "\n" << sql);
            jassertfalse;
            stmt = nullptr;
        }
    }
    ~Statement() { if (stmt) sqlite3_finalize (stmt); }

    bool ok() const noexcept { return stmt != nullptr; }
    Statement& bind (int idx, juce::int64 v)              { if (stmt) sqlite3_bind_int64 (stmt, idx, v); return *this; }
    Statement& bind (int idx, int v)                      { return bind (idx, (juce::int64) v); }
    Statement& bind (int idx, double v)                   { if (stmt) sqlite3_bind_double (stmt, idx, v); return *this; }
    Statement& bind (int idx, const juce::String& v)      { if (stmt) sqlite3_bind_text (stmt, idx, v.toRawUTF8(), -1, SQLITE_TRANSIENT); return *this; }
    Statement& bindBlob (int idx, const void* d, int n)   { if (stmt) sqlite3_bind_blob (stmt, idx, d, n, SQLITE_TRANSIENT); return *this; }
    Statement& bindNull (int idx)                         { if (stmt) sqlite3_bind_null (stmt, idx); return *this; }
    Statement& bindVar (int idx, const juce::var& v)
    {
        if (v.isString())                       return bind (idx, v.toString());
        if (v.isDouble())                       return bind (idx, (double) v);
        if (v.isInt() || v.isInt64() || v.isBool()) return bind (idx, (juce::int64) v);
        return bindNull (idx);
    }

    bool step()
    {
        if (! stmt) return false;
        const int rc = sqlite3_step (stmt);
        if (rc == SQLITE_ROW) return true;
        if (rc != SQLITE_DONE) { owner.lastError = sqlite3_errmsg (owner.db); DBG ("SQL step error: " << owner.lastError); }
        return false;
    }
    void run() { step(); }

    juce::int64 i64 (int col) const  { return sqlite3_column_int64 (stmt, col); }
    int i32 (int col) const          { return sqlite3_column_int (stmt, col); }
    double dbl (int col) const       { return sqlite3_column_double (stmt, col); }
    bool isNull (int col) const      { return sqlite3_column_type (stmt, col) == SQLITE_NULL; }
    juce::String str (int col) const
    {
        auto* t = (const char*) sqlite3_column_text (stmt, col);
        return t ? juce::String::fromUTF8 (t) : juce::String();
    }
    const void* blob (int col, int& size) const { size = sqlite3_column_bytes (stmt, col); return sqlite3_column_blob (stmt, col); }

private:
    Database& owner;
    sqlite3_stmt* stmt = nullptr;
};

//==============================================================================
Database::Database (const juce::File& file) : dbFile (file)
{
    file.getParentDirectory().createDirectory();
    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2 (file.getFullPathName().toRawUTF8(), &db, flags, nullptr) != SQLITE_OK)
    {
        lastError = db ? sqlite3_errmsg (db) : "cannot open database";
        if (db) sqlite3_close (db);
        db = nullptr;
        return;
    }
    sqlite3_busy_timeout (db, 5000);
    exec ("PRAGMA journal_mode=WAL;");
    exec ("PRAGMA synchronous=NORMAL;");
    exec ("PRAGMA foreign_keys=ON;");
    exec ("PRAGMA temp_store=MEMORY;");
    migrate();
}

Database::~Database()
{
    if (db) sqlite3_close (db);
}

bool Database::exec (const char* sql)
{
    char* err = nullptr;
    if (sqlite3_exec (db, sql, nullptr, nullptr, &err) != SQLITE_OK)
    {
        lastError = err ? err : "error";
        DBG ("SQL exec error: " << lastError << "\n" << sql);
        sqlite3_free (err);
        return false;
    }
    return true;
}

void Database::migrate()
{
    const juce::ScopedLock sl (lock);
    int version = 0;
    {
        Statement st (*this, "PRAGMA user_version;");
        if (st.step()) version = st.i32 (0);
    }
    if (version < 1)
    {
        exec ("BEGIN IMMEDIATE;");
        exec (R"SQL(
CREATE TABLE IF NOT EXISTS libraries (
    id INTEGER PRIMARY KEY, path TEXT NOT NULL UNIQUE, name TEXT NOT NULL, added_at INTEGER NOT NULL);

CREATE TABLE IF NOT EXISTS samples (
    id INTEGER PRIMARY KEY,
    library_id INTEGER NOT NULL REFERENCES libraries(id) ON DELETE CASCADE,
    path TEXT NOT NULL UNIQUE,
    filename TEXT NOT NULL,
    file_size INTEGER DEFAULT 0, mtime INTEGER DEFAULT 0, added_at INTEGER NOT NULL,
    duration REAL DEFAULT 0, effective_length REAL DEFAULT 0,
    sample_rate REAL DEFAULT 0, bit_depth INTEGER DEFAULT 0, channels INTEGER DEFAULT 0, codec TEXT DEFAULT '',
    bpm REAL DEFAULT 0, bpm_manual INTEGER DEFAULT 0,
    key_int INTEGER DEFAULT -1, key_manual INTEGER DEFAULT 0,
    is_loop INTEGER DEFAULT 0, type_manual INTEGER DEFAULT 0,
    peak REAL DEFAULT 0, rms REAL DEFAULT 0, centroid REAL DEFAULT 0,
    fingerprint TEXT,
    favourite INTEGER DEFAULT 0,
    analysed INTEGER DEFAULT 0,          -- 0 pending, 1 done, -1 failed
    waveform BLOB);
CREATE INDEX IF NOT EXISTS idx_samples_lib ON samples(library_id);
CREATE INDEX IF NOT EXISTS idx_samples_fp ON samples(fingerprint);
CREATE INDEX IF NOT EXISTS idx_samples_analysed ON samples(analysed);
CREATE INDEX IF NOT EXISTS idx_samples_bpm ON samples(bpm);
CREATE INDEX IF NOT EXISTS idx_samples_key ON samples(key_int);

CREATE TABLE IF NOT EXISTS tags (
    id INTEGER PRIMARY KEY, name TEXT NOT NULL COLLATE NOCASE, type INTEGER NOT NULL DEFAULT 3,
    UNIQUE(name, type));

CREATE TABLE IF NOT EXISTS sample_tags (
    sample_id INTEGER NOT NULL REFERENCES samples(id) ON DELETE CASCADE,
    tag_id INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,
    auto INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (sample_id, tag_id)) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS idx_st_tag ON sample_tags(tag_id);

CREATE TABLE IF NOT EXISTS collections (id INTEGER PRIMARY KEY, name TEXT NOT NULL, created_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS collection_samples (
    collection_id INTEGER NOT NULL REFERENCES collections(id) ON DELETE CASCADE,
    sample_id INTEGER NOT NULL REFERENCES samples(id) ON DELETE CASCADE,
    added_at INTEGER NOT NULL,
    PRIMARY KEY (collection_id, sample_id)) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS saved_searches (id INTEGER PRIMARY KEY, name TEXT NOT NULL, query TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT);
PRAGMA user_version = 1;
)SQL");
        exec ("COMMIT;");
    }
}

juce::int64 Database::dataVersion()
{
    const juce::ScopedLock sl (lock);
    Statement st (*this, "PRAGMA data_version;");
    return st.step() ? st.i64 (0) : 0;
}

Database::Transaction::Transaction (Database& d) : db (d) { db.lock.enter(); db.exec ("BEGIN IMMEDIATE;"); }
Database::Transaction::~Transaction() { if (! done) db.exec ("ROLLBACK;"); db.lock.exit(); }
void Database::Transaction::commit() { if (! done) { db.exec ("COMMIT;"); done = true; db.touched(); } }

static juce::int64 nowMs() { return juce::Time::currentTimeMillis(); }

//==============================================================================
juce::int64 Database::addLibrary (const juce::File& folder)
{
    const juce::ScopedLock sl (lock);
    Statement ins (*this, "INSERT OR IGNORE INTO libraries(path, name, added_at) VALUES (?,?,?);");
    ins.bind (1, folder.getFullPathName()).bind (2, folder.getFileName()).bind (3, nowMs()).run();
    Statement sel (*this, "SELECT id FROM libraries WHERE path = ?;");
    sel.bind (1, folder.getFullPathName());
    touched();
    return sel.step() ? sel.i64 (0) : 0;
}

void Database::removeLibrary (juce::int64 id)
{
    const juce::ScopedLock sl (lock);
    Statement (*this, "DELETE FROM libraries WHERE id = ?;").bind (1, id).run();
    exec ("DELETE FROM tags WHERE id NOT IN (SELECT DISTINCT tag_id FROM sample_tags) AND type = 2;");
    touched();
}

std::vector<LibraryRow> Database::libraries()
{
    const juce::ScopedLock sl (lock);
    std::vector<LibraryRow> out;
    Statement st (*this, "SELECT l.id, l.path, l.name, (SELECT COUNT(*) FROM samples s WHERE s.library_id = l.id) "
                         "FROM libraries l ORDER BY l.name COLLATE NOCASE;");
    while (st.step()) out.push_back ({ st.i64 (0), st.str (1), st.str (2), st.i32 (3) });
    return out;
}

//==============================================================================
juce::int64 Database::upsertSampleFile (juce::int64 libraryId, const juce::File& file, juce::int64 size, juce::int64 mtime)
{
    const juce::ScopedLock sl (lock);
    const auto path = file.getFullPathName();
    Statement sel (*this, "SELECT id, file_size, mtime FROM samples WHERE path = ?;");
    sel.bind (1, path);
    if (sel.step())
    {
        const auto id = sel.i64 (0);
        if (sel.i64 (1) != size || sel.i64 (2) != mtime)
        {
            Statement up (*this, "UPDATE samples SET file_size = ?, mtime = ?, analysed = 0, library_id = ? WHERE id = ?;");
            up.bind (1, size).bind (2, mtime).bind (3, libraryId).bind (4, id).run();
            touched();
        }
        return id;
    }
    Statement ins (*this, "INSERT INTO samples(library_id, path, filename, file_size, mtime, added_at, codec) VALUES (?,?,?,?,?,?,?);");
    ins.bind (1, libraryId).bind (2, path).bind (3, file.getFileNameWithoutExtension())
       .bind (4, size).bind (5, mtime).bind (6, nowMs())
       .bind (7, file.getFileExtension().trimCharactersAtStart (".").toUpperCase()).run();
    touched();
    return sqlite3_last_insert_rowid (db);
}

int Database::removeMissing (juce::int64 libraryId, const std::set<juce::String>& seen)
{
    const juce::ScopedLock sl (lock);
    juce::Array<juce::int64> toDelete;
    {
        Statement st (*this, "SELECT id, path FROM samples WHERE library_id = ?;");
        st.bind (1, libraryId);
        while (st.step())
            if (seen.find (st.str (1)) == seen.end()) toDelete.add (st.i64 (0));
    }
    for (auto id : toDelete)
        Statement (*this, "DELETE FROM samples WHERE id = ?;").bind (1, id).run();
    if (! toDelete.isEmpty()) touched();
    return toDelete.size();
}

std::vector<std::pair<juce::int64, juce::String>> Database::samplesNeedingAnalysis (int limit)
{
    const juce::ScopedLock sl (lock);
    std::vector<std::pair<juce::int64, juce::String>> out;
    Statement st (*this, "SELECT id, path FROM samples WHERE analysed = 0 ORDER BY id LIMIT ?;");
    st.bind (1, limit);
    while (st.step()) out.emplace_back (st.i64 (0), st.str (1));
    return out;
}

void Database::storeAnalysis (juce::int64 id, const AnalysisResult& r, const juce::Array<TagSuggestion>& autoTags)
{
    Transaction t (*this);
    {
        Statement st (*this, R"SQL(
UPDATE samples SET
    duration = ?1, effective_length = ?2, sample_rate = ?3, bit_depth = ?4, channels = ?5,
    codec = CASE WHEN ?6 = '' THEN codec ELSE ?6 END,
    bpm = CASE WHEN bpm_manual THEN bpm ELSE ?7 END,
    key_int = CASE WHEN key_manual THEN key_int ELSE ?8 END,
    is_loop = CASE WHEN type_manual THEN is_loop ELSE ?9 END,
    peak = ?10, rms = ?11, centroid = ?12, fingerprint = ?13, waveform = ?14, analysed = 1
WHERE id = ?15;)SQL");
        auto codec = r.codec.replace (" file", "").replace ("Audio Interchange File Format", "AIFF")
                            .replace ("WAV", "WAV").trim();
        st.bind (1, r.durationSeconds).bind (2, r.effectiveSeconds).bind (3, r.sampleRate)
          .bind (4, r.bitDepth).bind (5, r.channels).bind (6, codec)
          .bind (7, r.bpm).bind (8, r.key.toInt()).bind (9, r.isLoop ? 1 : 0)
          .bind (10, (double) r.peak).bind (11, (double) r.rms).bind (12, (double) r.spectralCentroid)
          .bind (13, r.fingerprint);
        if (r.overview.isEmpty()) st.bindNull (14);
        else st.bindBlob (14, r.overview.data.data(), (int) r.overview.data.size());
        st.bind (15, id).run();
    }
    // replace automatic tags, keep the user's own
    Statement (*this, "DELETE FROM sample_tags WHERE sample_id = ? AND auto = 1;").bind (1, id).run();
    for (auto& tag : autoTags)
    {
        Statement ins (*this, "INSERT OR IGNORE INTO tags(name, type) VALUES (?, ?);");
        ins.bind (1, tag.name).bind (2, (int) tag.type).run();
        Statement link (*this, "INSERT OR IGNORE INTO sample_tags(sample_id, tag_id, auto) "
                               "SELECT ?, id, 1 FROM tags WHERE name = ? AND type = ?;");
        link.bind (1, id).bind (2, tag.name).bind (3, (int) tag.type).run();
    }
    t.commit();
}

void Database::markAnalysisFailed (juce::int64 id)
{
    const juce::ScopedLock sl (lock);
    Statement (*this, "UPDATE samples SET analysed = -1 WHERE id = ?;").bind (1, id).run();
    touched();
}

void Database::requestReanalysis (const juce::Array<juce::int64>& ids)
{
    Transaction t (*this);
    for (auto id : ids)
        Statement (*this, "UPDATE samples SET analysed = 0, bpm_manual = 0, key_manual = 0, type_manual = 0 WHERE id = ?;").bind (1, id).run();
    t.commit();
}

//==============================================================================
static const char* sampleColumns =
    "s.id, s.library_id, s.path, s.filename, l.name, s.codec, "
    "(SELECT group_concat(t.name, ', ') FROM sample_tags st JOIN tags t ON t.id = st.tag_id WHERE st.sample_id = s.id AND t.type <> 2), "
    "s.file_size, s.added_at, s.duration, s.effective_length, s.sample_rate, s.bpm, s.bit_depth, s.channels, "
    "s.key_int, s.is_loop, s.favourite, s.analysed, s.fingerprint, "
    "(s.fingerprint IS NOT NULL AND EXISTS (SELECT 1 FROM samples d WHERE d.fingerprint = s.fingerprint AND d.id <> s.id))";

std::vector<SampleRow> Database::runSampleQuery (const juce::String& sql, const juce::Array<juce::var>& binds)
{
    std::vector<SampleRow> out;
    Statement st (*this, sql);
    for (int i = 0; i < binds.size(); ++i) st.bindVar (i + 1, binds[i]);
    while (st.step())
    {
        SampleRow r;
        r.id = st.i64 (0); r.libraryId = st.i64 (1); r.path = st.str (2); r.filename = st.str (3);
        r.libraryName = st.str (4); r.codec = st.str (5); r.tags = st.str (6);
        r.fileSize = st.i64 (7); r.addedAt = st.i64 (8); r.duration = st.dbl (9); r.effectiveLength = st.dbl (10);
        r.sampleRate = st.dbl (11); r.bpm = st.dbl (12); r.bitDepth = st.i32 (13); r.channels = st.i32 (14);
        r.key = MusicalKey::fromInt (st.i32 (15)); r.isLoop = st.i32 (16) != 0; r.favourite = st.i32 (17) != 0;
        r.analysed = st.i32 (18) == 1; r.fingerprint = st.str (19); r.hasDuplicate = st.i32 (20) != 0;
        out.push_back (std::move (r));
    }
    return out;
}

std::optional<SampleRow> Database::sample (juce::int64 id)
{
    const juce::ScopedLock sl (lock);
    auto rows = runSampleQuery (juce::String ("SELECT ") + sampleColumns
                                + " FROM samples s JOIN libraries l ON l.id = s.library_id WHERE s.id = ?;",
                                { juce::var (id) });
    if (rows.empty()) return std::nullopt;
    return rows.front();
}

static juce::String escapeLike (const juce::String& s)
{
    return s.replace ("\\", "\\\\").replace ("%", "\\%").replace ("_", "\\_");
}

juce::String Database::buildWhere (const SearchQuery& q, juce::Array<juce::var>& binds) const
{
    juce::StringArray c;
    c.add ("1=1");

    for (auto& word : juce::StringArray::fromTokens (q.text, " ", "\""))
    {
        auto w = word.trim().unquoted();
        if (w.isEmpty()) continue;
        const bool negate = w.startsWithChar ('-') && w.length() > 1;
        if (negate) w = w.substring (1);
        auto like = "%" + escapeLike (w) + "%";
        juce::String expr = "(s.filename LIKE ? ESCAPE '\\' OR s.path LIKE ? ESCAPE '\\' OR EXISTS (SELECT 1 FROM sample_tags st JOIN tags t ON t.id = st.tag_id "
                            "WHERE st.sample_id = s.id AND t.name LIKE ? ESCAPE '\\'))";
        c.add (negate ? "NOT " + expr : expr);
        binds.add (like); binds.add (like); binds.add (like);
    }

    if (! q.includeTags.isEmpty())
    {
        if (q.tagsMatchAll)
        {
            for (auto t : q.includeTags)
            {
                c.add ("EXISTS (SELECT 1 FROM sample_tags st WHERE st.sample_id = s.id AND st.tag_id = ?)");
                binds.add (t);
            }
        }
        else
        {
            juce::StringArray qs;
            for (auto t : q.includeTags) { qs.add ("?"); binds.add (t); }
            c.add ("EXISTS (SELECT 1 FROM sample_tags st WHERE st.sample_id = s.id AND st.tag_id IN (" + qs.joinIntoString (",") + "))");
        }
    }
    for (auto t : q.excludeTags)
    {
        c.add ("NOT EXISTS (SELECT 1 FROM sample_tags st WHERE st.sample_id = s.id AND st.tag_id = ?)");
        binds.add (t);
    }
    if (q.type == 1) c.add ("s.is_loop = 1");
    if (q.type == 2) c.add ("s.is_loop = 0");
    if (q.bpmMin > 0) { c.add ("s.bpm >= ?"); binds.add (q.bpmMin - 0.005); }
    if (q.bpmMax > 0) { c.add ("s.bpm <= ? AND s.bpm > 0"); binds.add (q.bpmMax + 0.005); }
    if (! q.keys.isEmpty())
    {
        // a key filter also matches one-shots whose note is the key's root
        juce::StringArray qs;
        for (auto k : q.keys)
        {
            qs.add ("?"); binds.add (k);
            auto mk = MusicalKey::fromInt (k);
            if (mk.isValid() && mk.mode != MusicalKey::Note) { qs.add ("?"); binds.add (mk.root + 24); }
        }
        c.add ("s.key_int IN (" + qs.joinIntoString (",") + ")");
    }
    if (q.collectionId != 0)
    {
        c.add ("EXISTS (SELECT 1 FROM collection_samples cs WHERE cs.sample_id = s.id AND cs.collection_id = ?)");
        binds.add (q.collectionId);
    }
    if (q.libraryId != 0) { c.add ("s.library_id = ?"); binds.add (q.libraryId); }
    if (q.favouritesOnly) c.add ("s.favourite = 1");
    if (q.hideDuplicates)
        c.add ("(s.fingerprint IS NULL OR s.id = (SELECT MIN(d.id) FROM samples d WHERE d.fingerprint = s.fingerprint))");
    if (q.similarTo != 0)
    {
        c.add ("s.id <> ? AND EXISTS (SELECT 1 FROM sample_tags a JOIN sample_tags b ON a.tag_id = b.tag_id JOIN tags t ON t.id = a.tag_id "
               "WHERE a.sample_id = s.id AND b.sample_id = ? AND t.type <> 2)");
        binds.add (q.similarTo); binds.add (q.similarTo);
    }
    return c.joinIntoString (" AND ");
}

static juce::String orderByFor (const SearchQuery& q)
{
    static const std::map<juce::String, juce::String> cols {
        { "name", "s.filename COLLATE NOCASE" }, { "type", "s.is_loop" }, { "key", "CASE WHEN s.key_int < 0 THEN 99 ELSE s.key_int % 12 END, s.key_int" },
        { "bpm", "CASE WHEN s.bpm <= 0 THEN 9999 ELSE s.bpm END" }, { "length", "s.duration" }, { "effective", "s.effective_length" },
        { "size", "s.file_size" }, { "date", "s.added_at" }, { "path", "s.path COLLATE NOCASE" },
        { "library", "l.name COLLATE NOCASE" }, { "bitdepth", "s.bit_depth" }, { "samplerate", "s.sample_rate" },
        { "channels", "s.channels" }, { "codec", "s.codec" }, { "fav", "s.favourite" }, { "tags", "7" } };
    auto it = cols.find (q.sortColumn);
    auto col = it != cols.end() ? it->second : juce::String ("s.filename COLLATE NOCASE");
    auto dir = juce::String (q.ascending ? " ASC" : " DESC");
    // apply direction to every part of a compound expression
    auto parts = juce::StringArray::fromTokens (col, ",", "");
    for (auto& p : parts) p = p.trim() + dir;
    return parts.joinIntoString (", ") + ", s.id";
}

std::vector<SampleRow> Database::search (const SearchQuery& q)
{
    const juce::ScopedLock sl (lock);
    juce::Array<juce::var> binds;
    auto where = buildWhere (q, binds);
    juce::String order;
    if (q.similarTo != 0)
    {
        // rank by weighted shared tags (type 3x, genre 2x, custom 2x), then same loop/one-shot, then BPM distance
        order = "(SELECT SUM(CASE t.type WHEN 0 THEN 3 WHEN 1 THEN 2 WHEN 3 THEN 2 ELSE 0 END) FROM sample_tags a JOIN sample_tags b ON a.tag_id = b.tag_id "
                "JOIN tags t ON t.id = a.tag_id WHERE a.sample_id = s.id AND b.sample_id = ?) "
                "+ (s.is_loop = (SELECT is_loop FROM samples WHERE id = ?)) DESC, "
                "ABS(s.bpm - (SELECT bpm FROM samples WHERE id = ?)), s.id";
        binds.add (q.similarTo); binds.add (q.similarTo); binds.add (q.similarTo);
    }
    else
        order = orderByFor (q);

    auto sql = juce::String ("SELECT ") + sampleColumns
             + " FROM samples s JOIN libraries l ON l.id = s.library_id WHERE " + where
             + " ORDER BY " + order + " LIMIT " + juce::String (juce::jmax (1, q.limit)) + ";";
    return runSampleQuery (sql, binds);
}

juce::int64 Database::randomSample (const SearchQuery& q, juce::int64 excludeId)
{
    const juce::ScopedLock sl (lock);
    juce::Array<juce::var> binds;
    auto where = buildWhere (q, binds);
    Statement st (*this, "SELECT s.id FROM samples s JOIN libraries l ON l.id = s.library_id WHERE " + where
                         + " AND s.id <> ? ORDER BY RANDOM() LIMIT 1;");
    int i = 1;
    for (auto& b : binds) st.bindVar (i++, b);
    st.bind (i, excludeId);
    return st.step() ? st.i64 (0) : 0;
}

std::vector<std::vector<juce::int64>> Database::duplicateGroups()
{
    const juce::ScopedLock sl (lock);
    std::vector<std::vector<juce::int64>> groups;
    Statement st (*this, "SELECT fingerprint, id FROM samples WHERE fingerprint IN "
                         "(SELECT fingerprint FROM samples WHERE fingerprint IS NOT NULL GROUP BY fingerprint HAVING COUNT(*) > 1) "
                         "ORDER BY fingerprint, id;");
    juce::String last;
    while (st.step())
    {
        auto fp = st.str (0);
        if (groups.empty() || fp != last) { groups.emplace_back(); last = fp; }
        groups.back().push_back (st.i64 (1));
    }
    return groups;
}

WaveformOverview Database::overview (juce::int64 id)
{
    const juce::ScopedLock sl (lock);
    Statement st (*this, "SELECT waveform FROM samples WHERE id = ?;");
    st.bind (1, id);
    if (st.step() && ! st.isNull (0))
    {
        int n = 0;
        auto* d = st.blob (0, n);
        return WaveformOverview::fromMemoryBlock (d, (size_t) n);
    }
    return {};
}

int Database::sampleCount()
{
    const juce::ScopedLock sl (lock);
    Statement st (*this, "SELECT COUNT(*) FROM samples;");
    return st.step() ? st.i32 (0) : 0;
}

int Database::pendingAnalysisCount()
{
    const juce::ScopedLock sl (lock);
    Statement st (*this, "SELECT COUNT(*) FROM samples WHERE analysed = 0;");
    return st.step() ? st.i32 (0) : 0;
}

//==============================================================================
void Database::setFavourite (const juce::Array<juce::int64>& ids, bool fav)
{
    Transaction t (*this);
    for (auto id : ids) Statement (*this, "UPDATE samples SET favourite = ? WHERE id = ?;").bind (1, fav ? 1 : 0).bind (2, id).run();
    t.commit();
}

void Database::setBpm (const juce::Array<juce::int64>& ids, double bpm)
{
    Transaction t (*this);
    for (auto id : ids) Statement (*this, "UPDATE samples SET bpm = ?, bpm_manual = 1 WHERE id = ?;").bind (1, bpm).bind (2, id).run();
    t.commit();
}

void Database::setKey (const juce::Array<juce::int64>& ids, MusicalKey key)
{
    Transaction t (*this);
    for (auto id : ids) Statement (*this, "UPDATE samples SET key_int = ?, key_manual = 1 WHERE id = ?;").bind (1, key.toInt()).bind (2, id).run();
    t.commit();
}

void Database::setLoop (const juce::Array<juce::int64>& ids, bool isLoop)
{
    Transaction t (*this);
    for (auto id : ids) Statement (*this, "UPDATE samples SET is_loop = ?, type_manual = 1 WHERE id = ?;").bind (1, isLoop ? 1 : 0).bind (2, id).run();
    t.commit();
}

//==============================================================================
juce::int64 Database::tagId (const juce::String& name, TagType type, bool create)
{
    const juce::ScopedLock sl (lock);
    auto n = name.trim();
    if (n.isEmpty()) return 0;
    if (create)
    {
        Statement (*this, "INSERT OR IGNORE INTO tags(name, type) VALUES (?, ?);").bind (1, n).bind (2, (int) type).run();
        touched();
    }
    Statement st (*this, "SELECT id FROM tags WHERE name = ? AND type = ?;");
    st.bind (1, n).bind (2, (int) type);
    return st.step() ? st.i64 (0) : 0;
}

std::vector<TagRow> Database::tags (bool onlyUsed)
{
    const juce::ScopedLock sl (lock);
    std::vector<TagRow> out;
    Statement st (*this, juce::String ("SELECT t.id, t.name, t.type, (SELECT COUNT(*) FROM sample_tags st WHERE st.tag_id = t.id) AS c FROM tags t ")
                         + (onlyUsed ? "WHERE c > 0 " : "") + "ORDER BY t.type, c DESC, t.name COLLATE NOCASE;");
    while (st.step()) out.push_back ({ st.i64 (0), st.str (1), (TagType) st.i32 (2), st.i32 (3) });
    return out;
}

std::vector<TagRow> Database::tagsFor (juce::int64 sampleId)
{
    const juce::ScopedLock sl (lock);
    std::vector<TagRow> out;
    Statement st (*this, "SELECT t.id, t.name, t.type, st.auto FROM sample_tags st JOIN tags t ON t.id = st.tag_id "
                         "WHERE st.sample_id = ? ORDER BY t.type, t.name COLLATE NOCASE;");
    st.bind (1, sampleId);
    while (st.step()) out.push_back ({ st.i64 (0), st.str (1), (TagType) st.i32 (2), st.i32 (3) });
    return out;
}

void Database::addTag (const juce::Array<juce::int64>& ids, juce::int64 tag)
{
    Transaction t (*this);
    for (auto id : ids)
    {
        // a tag the user adds by hand becomes "manual" so re-analysis keeps it
        Statement (*this, "INSERT INTO sample_tags(sample_id, tag_id, auto) VALUES (?, ?, 0) "
                          "ON CONFLICT(sample_id, tag_id) DO UPDATE SET auto = 0;").bind (1, id).bind (2, tag).run();
    }
    t.commit();
}

void Database::removeTag (const juce::Array<juce::int64>& ids, juce::int64 tag)
{
    Transaction t (*this);
    for (auto id : ids) Statement (*this, "DELETE FROM sample_tags WHERE sample_id = ? AND tag_id = ?;").bind (1, id).bind (2, tag).run();
    t.commit();
}

void Database::deleteTag (juce::int64 tag)
{
    const juce::ScopedLock sl (lock);
    Statement (*this, "DELETE FROM tags WHERE id = ?;").bind (1, tag).run();
    touched();
}

void Database::renameTag (juce::int64 tag, const juce::String& newName)
{
    const juce::ScopedLock sl (lock);
    Statement (*this, "UPDATE OR IGNORE tags SET name = ? WHERE id = ?;").bind (1, newName.trim()).bind (2, tag).run();
    touched();
}

//==============================================================================
juce::int64 Database::createCollection (const juce::String& name)
{
    const juce::ScopedLock sl (lock);
    Statement (*this, "INSERT INTO collections(name, created_at) VALUES (?, ?);").bind (1, name).bind (2, nowMs()).run();
    touched();
    return sqlite3_last_insert_rowid (db);
}

void Database::renameCollection (juce::int64 id, const juce::String& name)
{
    const juce::ScopedLock sl (lock);
    Statement (*this, "UPDATE collections SET name = ? WHERE id = ?;").bind (1, name).bind (2, id).run();
    touched();
}

void Database::deleteCollection (juce::int64 id)
{
    const juce::ScopedLock sl (lock);
    Statement (*this, "DELETE FROM collections WHERE id = ?;").bind (1, id).run();
    touched();
}

std::vector<CollectionRow> Database::collections()
{
    const juce::ScopedLock sl (lock);
    std::vector<CollectionRow> out;
    Statement st (*this, "SELECT c.id, c.name, (SELECT COUNT(*) FROM collection_samples cs WHERE cs.collection_id = c.id) "
                         "FROM collections c ORDER BY c.name COLLATE NOCASE;");
    while (st.step()) out.push_back ({ st.i64 (0), st.str (1), st.i32 (2) });
    return out;
}

void Database::addToCollection (juce::int64 col, const juce::Array<juce::int64>& ids)
{
    Transaction t (*this);
    for (auto id : ids)
        Statement (*this, "INSERT OR IGNORE INTO collection_samples(collection_id, sample_id, added_at) VALUES (?, ?, ?);")
            .bind (1, col).bind (2, id).bind (3, nowMs()).run();
    t.commit();
}

void Database::removeFromCollection (juce::int64 col, const juce::Array<juce::int64>& ids)
{
    Transaction t (*this);
    for (auto id : ids)
        Statement (*this, "DELETE FROM collection_samples WHERE collection_id = ? AND sample_id = ?;").bind (1, col).bind (2, id).run();
    t.commit();
}

//==============================================================================
juce::int64 Database::saveSearch (const juce::String& name, const SearchQuery& q)
{
    const juce::ScopedLock sl (lock);
    auto copy = q; copy.title = name;
    Statement (*this, "INSERT INTO saved_searches(name, query) VALUES (?, ?);").bind (1, name).bind (2, copy.toJson()).run();
    touched();
    return sqlite3_last_insert_rowid (db);
}

void Database::deleteSavedSearch (juce::int64 id)
{
    const juce::ScopedLock sl (lock);
    Statement (*this, "DELETE FROM saved_searches WHERE id = ?;").bind (1, id).run();
    touched();
}

std::vector<SavedSearchRow> Database::savedSearches()
{
    const juce::ScopedLock sl (lock);
    std::vector<SavedSearchRow> out;
    Statement st (*this, "SELECT id, name, query FROM saved_searches ORDER BY name COLLATE NOCASE;");
    while (st.step()) out.push_back ({ st.i64 (0), st.str (1), st.str (2) });
    return out;
}

juce::String Database::getSetting (const juce::String& key, const juce::String& fallback)
{
    const juce::ScopedLock sl (lock);
    Statement st (*this, "SELECT value FROM settings WHERE key = ?;");
    st.bind (1, key);
    return st.step() ? st.str (0) : fallback;
}

void Database::setSetting (const juce::String& key, const juce::String& value)
{
    const juce::ScopedLock sl (lock);
    Statement (*this, "INSERT INTO settings(key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value;")
        .bind (1, key).bind (2, value).run();
    touched();
}
}
