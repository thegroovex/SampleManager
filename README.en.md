# SampleManager

**Open-source sample manager as a standalone app and plugin (VST3 / AU), built with JUCE.**
Inspired by the feature list of ADSR Sample Manager, written from scratch and free (AGPLv3).
No account, no cloud: everything runs locally.

[Deutsche Version](README.md)

## Features

- **One library for everything**: the standalone app and every plugin instance share one SQLite database
  (WAL mode, several processes at once). Changes show up in every open window.
- **Automatic background analysis**: key (chromagram + Krumhansl profiles, single notes for one-shots),
  BPM (beat-grid fit over the loop length, onset autocorrelation), loop vs one-shot, effective length, spectrum.
  Hints in file names (`Loop_128_Am`, `90bpm`, `F#min`, `8A`) take priority.
- **Auto tags** by sound type (kick, snare, hi-hat, bass, pad, vocal, FX …), genre and label/pack, including
  Splice, Loopcloud and Noiiz folder layouts. Custom tags, rename, delete.
- **Search**: free text (`-word` excludes), tags with AND/OR and exclusion, type, key, BPM range, favourites,
  library, collection. **Multiple search tabs** per instance, **saved searches**.
- **Similar sounds** (weighted shared tags), **random** + **random back**.
- **Duplicate detection** (format-independent loudness fingerprint) with hide-duplicates.
- **Favourite collections** – drag samples onto a collection.
- **Preview in context**: loops are stretched to the project tempo (SoundTouch, time-stretch or varispeed),
  **key sync** transposes to the project key, **quantised starts** (beat/bar, phase aligned),
  **MIDI input** plays the sample chromatically (C3 = original pitch).
- **Sample shaping**: trim start/end (snap to zero crossing, 1/16, beat), forward/reverse/ping-pong,
  high/low-pass, fade in/out, gain, normalise, level meter.
- **Waveform with frequency colouring** (bass blue, mids orange, highs white), zoom (+/−, pinch, Cmd+wheel), beat grid.
- **Drag & drop into your DAW** – raw file or rendered with tempo, key, fades and filters.
- **Tempo source**: host tempo in the plugin, **Ableton Link** or manual tempo in the standalone app.
- **MIDI learn** for random, random back, play/stop, next/previous, filters, fades, gain.
- Sortable columns (type, key, BPM, tags, length, effective length, size, date, path, library, bit depth,
  sample rate, channels, codec); right-click the header to show/hide columns.
- UI in **English or German** (settings).

### Keyboard

| Key | Action |
|---|---|
| Space / Enter | Play / stop |
| ↑ ↓ | Previous / next sample (auto-plays) |
| R / B | Random / random back |
| F | Toggle favourite |
| T | Edit tags |
| S | Find similar |
| Cmd/Ctrl + T / W / F | New tab / close tab / search |
| Alt + click a tag | Exclude tag |

## Download

Builds for macOS (universal) and Windows: [Releases](https://github.com/thegroovex/SampleManager/releases/latest).
Every push also builds macOS, Windows and Linux artefacts (GitHub Actions → *Build & Test*).

- **macOS**: `SampleManager.app` to `/Applications`, `SampleManager.vst3` to `~/Library/Audio/Plug-Ins/VST3`,
  `SampleManager.component` to `~/Library/Audio/Plug-Ins/Components`. Builds are not notarised:
  run `xattr -dr com.apple.quarantine <file>` once.
- **Windows**: `SampleManager.vst3` to `C:\Program Files\Common Files\VST3`, the `.exe` anywhere.

The database lives in `~/Library/Application Support/SampleManager/library.sqlite` (macOS),
`%APPDATA%\SampleManager\library.sqlite` (Windows) or `~/.config/SampleManager` (Linux).
Set `SAMPLEMANAGER_DATA_DIR` to move it.

## Building

Requirements: CMake ≥ 3.22, a C++17 compiler (Xcode, Visual Studio 2022, GCC/Clang), Git.
All dependencies are fetched automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Universal macOS build: `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.
Options: `-DSM_WITH_LINK=OFF`, `-DSM_BUILD_TESTS=OFF`, `-DSM_BUILD_PLUGIN=OFF` (core + tests only).

## Layout

```
Source/Core     database, analysis (key/BPM/loop), auto tagger, file name parser, scanner
Source/Audio    renderer (SoundTouch, filters, fades), real-time player, MIDI learn, tempo/Link
Source/UI       search & tabs, side panel, table, waveform, controls, tag editor, settings
Tests           unit tests with synthetic signals (chords, drum loops, noise)
```

## Dependencies & licence

[JUCE 8](https://juce.com) (AGPLv3), [SoundTouch](https://codeberg.org/soundtouch/soundtouch) (LGPL 2.1),
[Ableton Link](https://github.com/Ableton/link) (GPLv2+), [SQLite](https://sqlite.org) (public domain).

SampleManager is licensed under the **GNU AGPLv3** (see [LICENSE](LICENSE)).
Not affiliated with ADSR Sounds; "ADSR" is a trademark of its owner. The ADSR cloud library is not part of this
project – SampleManager works with your local folders (including Splice, Loopcloud, Noiiz).
