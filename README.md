# SampleManager

**Open-Source Sample-Manager als Standalone-App und Plugin (VST3 / AU) – gebaut mit JUCE.**
Inspiriert von der Funktionsliste des ADSR Sample Manager, komplett neu geschrieben und frei (AGPLv3).
Kein Account, keine Cloud: alles läuft lokal.

[English version](README.en.md)

## Funktionen

- **Eine Bibliothek für alle**: Standalone-App und jede Plugin-Instanz in der DAW nutzen dieselbe SQLite-Datenbank
  (WAL-Modus, mehrere Prozesse gleichzeitig). Änderungen erscheinen sofort in allen offenen Fenstern.
- **Automatische Analyse** im Hintergrund: Tonart (Chromagramm + Krumhansl-Profile, Einzelnoten bei One-Shots),
  BPM (Beat-Grid-Fit über die Looplänge, Onset-Autokorrelation), Loop/One-Shot, effektive Länge, Spektrum.
  Angaben im Dateinamen (`Loop_128_Am`, `90bpm`, `F#min`, `8A`) haben Vorrang.
- **Auto-Tags** nach Sound-Typ (Kick, Snare, Hi-Hat, Bass, Pad, Vocal, FX …), Genre und Label/Pack – inklusive
  Splice-, Loopcloud- und Noiiz-Ordnerstrukturen. Eigene Tags, Umbenennen, Löschen.
- **Suche**: Freitext (`-wort` schließt aus), Tags mit AND/OR und Ausschluss, Typ, Tonart, BPM-Bereich,
  Favoriten, Bibliothek, Sammlung. **Mehrere Such-Tabs** pro Instanz, **gespeicherte Suchen**.
- **Ähnliche Sounds** (gewichtete Tag-Übereinstimmung), **Zufall** + **Zufall zurück**.
- **Duplikate** erkennen (Lautheits-Fingerprint, formatunabhängig) und ausblenden.
- **Favoriten-Sammlungen** – Samples per Drag & Drop in Sammlungen ziehen.
- **Vorhören im Kontext**: Loops werden auf das Projekttempo gestretcht (SoundTouch, Time-Stretch oder Varispeed),
  **Tonart-Sync** transponiert auf die Projekt-Tonart, **quantisierte Starts** (Schlag/Takt, phasenrichtig),
  **MIDI-Eingang** spielt das Sample chromatisch (C3 = Originaltonhöhe).
- **Sample-Bearbeitung**: Start/Ende trimmen (Raster: Nulldurchgang, 1/16, Schlag), vorwärts/rückwärts/Ping-Pong,
  Hoch-/Tiefpass, Fade-In/-Out, Gain, Normalisieren, Pegelanzeige.
- **Wellenform mit Frequenzfarben** (Bass blau, Mitten orange, Höhen weiß), Zoom (+/−, Pinch, Cmd+Mausrad), Beat-Raster.
- **Drag & Drop in die DAW** – Original oder bearbeitet gerendert (Tempo, Tonart, Fades, Filter).
- **Tempo-Quelle**: im Plugin der Host, in der Standalone-App **Ableton Link** oder manuelles Tempo.
- **MIDI Learn** für Zufall, Zufall zurück, Play/Stop, Nächstes/Vorheriges, Filter, Fades, Gain.
- Sortierbare Spalten (Typ, Tonart, BPM, Tags, Länge, effektive Länge, Größe, Datum, Pfad, Bibliothek, Bittiefe,
  Samplerate, Kanäle, Codec), Spalten per Rechtsklick auf den Tabellenkopf ein-/ausblenden.
- Oberfläche auf **Deutsch oder Englisch** (Einstellungen).

### Tastatur

| Taste | Aktion |
|---|---|
| Leertaste / Enter | Play / Stop |
| ↑ ↓ | Vorheriges / nächstes Sample (spielt automatisch) |
| R / B | Zufall / Zufall zurück |
| F | Favorit an/aus |
| T | Tags bearbeiten |
| S | Ähnliche finden |
| Cmd/Strg + T / W / F | Neuer Tab / Tab schließen / Suche |
| Alt + Klick auf Tag | Tag ausschließen |

## Download

Fertige Builds für macOS (Universal) und Windows: [Releases](https://github.com/thegroovex/SampleManager/releases/latest).
Jeder Push baut zusätzlich Artefakte für macOS, Windows und Linux (GitHub Actions → *Build & Test*).

- **macOS**: `SampleManager.app` nach `/Programme`, `SampleManager.vst3` nach `~/Library/Audio/Plug-Ins/VST3`,
  `SampleManager.component` nach `~/Library/Audio/Plug-Ins/Components`. Die Builds sind nicht notarisiert:
  einmal `xattr -dr com.apple.quarantine <Datei>` ausführen.
- **Windows**: `SampleManager.vst3` nach `C:\Program Files\Common Files\VST3`, die `.exe` irgendwohin.

Die Datenbank liegt unter `~/Library/Application Support/SampleManager/library.sqlite` (macOS),
`%APPDATA%\SampleManager\library.sqlite` (Windows) bzw. `~/.config/SampleManager` (Linux).
Mit der Umgebungsvariable `SAMPLEMANAGER_DATA_DIR` lässt sie sich verlegen.

## Selbst bauen

Voraussetzungen: CMake ≥ 3.22, ein C++17-Compiler (Xcode, Visual Studio 2022, GCC/Clang), Git.
Alle Abhängigkeiten werden automatisch geladen.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Universal-Build auf dem Mac: `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.
Optionen: `-DSM_WITH_LINK=OFF`, `-DSM_BUILD_TESTS=OFF`, `-DSM_BUILD_PLUGIN=OFF` (nur Kern + Tests).

## Aufbau

```
Source/Core     Datenbank, Analyse (Key/BPM/Loop), Auto-Tagger, Dateinamen-Parser, Scanner
Source/Audio    Renderer (SoundTouch, Filter, Fades), Echtzeit-Player, MIDI Learn, Tempo/Link
Source/UI       Suche & Tabs, Seitenleiste, Tabelle, Wellenform, Regler, Tag-Editor, Einstellungen
Tests           Unit-Tests mit synthetischen Signalen (Akkorde, Drum-Loops, Noise)
```

## Abhängigkeiten & Lizenz

[JUCE 8](https://juce.com) (AGPLv3), [SoundTouch](https://codeberg.org/soundtouch/soundtouch) (LGPL 2.1),
[Ableton Link](https://github.com/Ableton/link) (GPLv2+), [SQLite](https://sqlite.org) (Public Domain).

SampleManager steht unter der **GNU AGPLv3** (siehe [LICENSE](LICENSE)).
Nicht verbunden mit ADSR Sounds; „ADSR“ ist eine Marke ihres Inhabers. Die ADSR-Cloud-Bibliothek ist nicht Teil
dieses Projekts – SampleManager arbeitet mit deinen lokalen Ordnern (auch Splice, Loopcloud, Noiiz).
