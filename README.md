# iso-dispatcher

Content-only ISO classifier and launcher (Qt6 C++ GUI + CLI).

## Purpose

Determine the platform/type of an `.iso` **exclusively from its ISO contents** (magic bytes, PVD, root-directory records, bounded token scan). No path- or folder-name rules are used for classification. When confidence is low, the app asks the user and saves the choice in a knowledge file for future runs.

## Main design rules

- **No path/folder classification**: the location on disk must not influence the decision.
- **Bounded reads**: no full ISO scan. Default: 64 KB fast header + PVD + root-directory seek + 2 MB token window + limited tree metadata.
- **No additional apt installation required**: Qt6 Core/Widgets + CMake + g++.
- **Config**: `$HOME/.config/iso-dispatcher.json` (created automatically on first run).
- **Knowledge**: JSON, in-memory `QHash`, batch save. SQLite is an optional future direction.
- **Safety**: the launcher refuses `xdg-open`/`gio open`/`kde-open` to prevent recursion.

## Measured accuracy (library test, 176 ISOs)

Measurements on `path:/`:

| Scan window | Correct | Accuracy | Notes |
|---|---:|---:|---|
| 64 KB | 129/176 | 73.3% | Console formats are usually identified already |
| 576 KB | 167/176 | 94.9% | 90% target achieved |
| 1 MB | 168/176 | 95.5% | |
| 2 MB | 169/176 | 96.0% | |
| 2 MB + PVD root-directory seek | ~175/176 | ~99.4% | No path rules needed |
| + bounded tree-walk metadata | 176/176 | 100% | Content-only |

Important: the remaining “stubborn” ISOs had markers beyond 2 MB, but the PVD root-directory pointer pointed directly to them. That is why the probe uses a **PVD root seek** instead of continuing to read linearly.

## Classification pipeline

Order (the strongest signals carry the most weight):

1. **Magic / file structure**: Wii magic `5D1C9EA3@0x1C`, 2352-sector PS1, UDF.
2. **Raw token scan** (bounded): `MICROSOFT*XBOX*MEDIA`, `DEFAULT.XBE`.
3. **PVD fields**: `system_id` (`PLAYSTATION`, `PSP GAME`, `AMIGA`), `volume_id`, `publisher`.
4. **Root-directory records** via PVD LBA: `PS3_DISC.SFB`, `SETUP.EXE`, `AUTORUN.INF`, `*.CAB`, `*.SH`.
5. **Bounded tree metadata** (`isoinfo -f`, max dirs): `*.EXE`, `*.INF`, `EBOOT.BIN`.
6. **Knowledge weights** from `knowledge.json` (capped by `learn.max_db_share`).
7. **Ask fallback** when the score is low or there is a tie.

## Known content signatures

| Class | Signature | Example application |
|---|---|---|
| Wii | `file`/Wii disc image magic | `dolphin-emu %f` |
| Xbox | `MICROSOFT*XBOX*MEDIA` + `DEFAULT.XBE` in the first 74 KB | `xemu %f` |
| PS1 | 2352 sectors + `BOOT =`/`SYSTEM.CNF` | `duckstation %f` |
| PS2 | PVD `system_id=PLAYSTATION` (+ optional `BOOT2 =`) | `pcsx2 %f` |
| PSP | PVD `system_id=PSP GAME` | `ppsspp %f` |
| PS3 | `PS3_DISC.SFB` in root/tree | `rpcs3 %f` |
| Amiga | PVD `system_id=AMIGA` | `fs-uae %f` |
| PC/UDF | UDF MIME, `SETUP.EXE/AUTORUN.INF/INSTALL.EXE`, `.CAB/.SH/.EXE` | `gnome-disk-image-mounter %f` |

## Build

```bash
cd /mnt/VG_00/PUBLIC-LIBRARY/SRC/Own_Code/Iso-dispatcher
cmake -S . -B build -G Ninja
cmake --build build -j
./build/iso-dispatcher --help
```

Requirements (available on the target system): Qt6 Core/Widgets 6.x, CMake, g++, ninja/make.

## CLI

```bash
iso-dispatcher                              # GUI
iso-dispatcher gui [file.iso]               # GUI with preload
iso-dispatcher classify <file.iso>          # JSON decision
iso-dispatcher why <file.iso>               # full score/evidence
iso-dispatcher launch <file.iso> [--class id] [--dry-run]
iso-dispatcher learn <dir> [--dry-run] [--limit N]
iso-dispatcher stats
iso-dispatcher decisions [N]
iso-dispatcher forget <fingerprint>
iso-dispatcher export <path.json>
iso-dispatcher config path|init|get|set
iso-dispatcher --help | -h | ?
```

Exit codes: `0=ok`, `1=general/usage error`, `2=marker invalid`, `3=entry/reference missing or invalid`, `4=class unknown/not configured`, `5=emulator/exec missing or launch failed`.

## Boot markers

`iso-dispatcher` supports portable boot markers (text files starting with `ISO-DISPATCHER-BOOT`).

Format:

```text
ISO-DISPATCHER-BOOT
class=ps1
entry=Game.cue
```

Validation before launch/classify decision:
- required `class` and `entry` (or known auto-resolve for PS2/PS3/PS4 layouts)
- `entry` must be relative/safe (no absolute path, no `..` traversal)
- entry must exist and be readable
- for `.cue` entries, referenced `FILE` targets are validated
- class alias: `psx -> ps1`

## GUI

- Select a file / drag it into the path field.
- `Scan`: displays evidence (magic, PVD, root, features) and the score table.
- `Launch selected`: starts the selected class; when confidence is low, it displays AskDialog.
- `Remember selected for this file`: saves the fingerprint-to-class mapping.

AskDialog displays the evidence, lets the user choose a class, and can remember the fingerprint/signature.

## Configuration (`$HOME/.config/iso-dispatcher.json`)

Created automatically with defaults. Important fields:

```json
{
  "scan": {
    "fast_bytes": 65536,
    "window_bytes": 2097152,
    "pvd_record_offset": 32924,
    "root_dir_bytes": 8192,
    "max_dirs": 64,
    "use_file_probe": true
  },
  "ask": {
    "enabled": true,
    "min_score": 70,
    "tie_margin": 10,
    "headless": "menu",
    "show_evidence": true,
    "remember_file": true,
    "remember_signature": false,
    "negative_cache_hours": 24
  },
  "learn": {
    "enabled": true,
    "db": "~/.local/share/iso-dispatcher/knowledge.json",
    "alpha": 1.0,
    "half_life_days": 180,
    "min_votes": 3,
    "auto_launch_weight": 0.0,
    "override_weight": 1.5,
    "max_db_share": 0.4,
    "target_accuracy": 0.90
  },
  "fallback": { "action": "ask", "else": ["gnome-disk-image-mounter", "%f"] },
  "classes": [ /* see default config in src/core/Config.cpp */ ]
}
```

`config set` supports dot-like keys through `/` in the internal implementation (`scan/window_bytes`). Use the exact key implemented by the CLI.

## Knowledge JSON

Default path: `~/.local/share/iso-dispatcher/knowledge.json`.

Structure:

```json
{
  "version": 1,
  "fingerprints": { "<fp>": "ps2" },
  "negative_cache": { "<fp>": 1760000000 },
  "weights": {
    "pvd.system_id=PLAYSTATION": {
      "ps2": { "pos": 8, "neg": 0, "updated": "..." }
    }
  },
  "decisions": [
    { "at": "...", "fingerprint": "...", "chosen": "ps2", "prompted": false, "overridden": false }
  ]
}
```

Weight: `log((pos + alpha) / (neg + alpha))`, active only when `pos+neg >= min_votes`. The database contribution is capped by `max_db_share` so it cannot override deterministic rules.

## Modules

| File | Role |
|---|---|
| `src/core/Types.h` | Data types: Feature, Rule, ClassDef, ProbeResult, Decision |
| `src/core/Util.*` | ISO string cleanup, wildcard matching, timestamps |
| `src/core/Config.*` | Load/save JSON config, defaults, get/set |
| `src/core/IsoProbe.*` | Content-only probe: magic, PVD, root-directory seek, bounded token/tree scan |
| `src/core/Classifier.*` | Rules + database weights + confidence/ask decision |
| `src/core/Knowledge.*` | JSON knowledge base, fingerprint overrides, negative cache, stats |
| `src/core/Launcher.*` | Detached launch with `%f/%e` substitution; no recursive openers |
| `src/cli/Cli.*` | CLI commands, help, terminal prompt |
| `src/ui/AskDialog.*` | Qt dialog for uncertain classifications |
| `src/ui/MainWindow.*` | GUI scan/score/launch/remember |
| `src/app/App.*` | GUI/CLI bootstrap, config/knowledge initialization |
| `src/main.cpp` | Entry point |

## Debugging

- `iso-dispatcher why file.iso` displays evidence and score breakdown.
- `iso-dispatcher stats` displays the knowledge overview.
- `iso-dispatcher decisions 20` displays recent choices/overrides.
- `iso-dispatcher learn <dir> --dry-run` tests batch classification without learning.
- Regression test: a Wii image in an oddly named folder must still be classified as Wii; the path must never take precedence.

## Known limitations / next steps

- `isoinfo -f` is used for bounded tree metadata; if `isoinfo` is missing, the probe falls back to root/PVD/token features.
- The current knowledge write mechanism uses simple JSON; migrate to SQLite for very large datasets (>50k).
- Extracted PS3 folders (`EBOOT.BIN`) are not ISOs; a separate folder handler may be added later.
- `config set` type detection is simple (bool/int/double/string).
- For distribution: test the `.desktop` file and MIME/handler integration separately; never run `xdg-open` on the same ISO from its handler.

---

# Nederlandse versie

# iso-dispatcher

Inhoudsgebaseerde ISO-classificatie en launcher (Qt6 C++ GUI + CLI).

## Doel

Bepaal het platform/type van een `.iso` **uitsluitend op basis van de ISO-inhoud** (magic bytes, PVD, root-directory-records en een begrensde token-scan). Er worden **geen pad- of mapnaamregels** gebruikt voor de classificatie. Bij een lage betrouwbaarheid vraagt de app de gebruiker om een keuze en slaat deze op in een kennisbestand voor volgende keren.

## Belangrijkste ontwerpregels

- **Geen classificatie op basis van pad/map**: de locatie op schijf mag de beslissing niet beïnvloeden.
- **Begrensde leesacties**: geen volledige ISO-scan. Standaard: 64 KB snelle header + PVD + seek naar de root-directory + 2 MB token-venster + beperkte tree-metadata.
- **Geen extra apt-installatie nodig**: Qt6 Core/Widgets + CMake + g++.
- **Configuratie**: `$HOME/.config/iso-dispatcher.json` (wordt automatisch aangemaakt bij de eerste start).
- **Kennis**: JSON, `QHash` in het geheugen en opslaan in batches. SQLite is een mogelijke uitbreiding voor later.
- **Veiligheid**: de launcher weigert `xdg-open`/`gio open`/`kde-open` om recursie te voorkomen.

## Gemeten nauwkeurigheid (bibliotheektest, 176 ISO's)

Metingen op `pad:/`:

| Scanvenster | Correct | Nauwkeurigheid | Opmerking |
|---|---:|---:|---|
| 64 KB | 129/176 | 73,3% | Consoleformaten worden meestal al herkend |
| 576 KB | 167/176 | 94,9% | Doel van 90% bereikt |
| 1 MB | 168/176 | 95,5% | |
| 2 MB | 169/176 | 96,0% | |
| 2 MB + PVD root-directory seek | ~175/176 | ~99,4% | Geen padregels nodig |
| + begrensde tree-walk-metadata | 176/176 | 100% | Alleen op basis van inhoud |

Belangrijk: de resterende “lastige” ISO's hadden markers buiten de eerste 2 MB, maar de root-directory-pointer in de PVD verwees er rechtstreeks naar. Daarom gebruikt de probe een **PVD root-seek** in plaats van lineair verder te lezen.

## Classificatieketen

Volgorde (de sterkste signalen wegen het zwaarst):

1. **Magic / bestandsstructuur**: Wii-magic `5D1C9EA3@0x1C`, PS1 met sectoren van 2352 bytes, UDF.
2. **Ruwe token-scan** (begrensd): `MICROSOFT*XBOX*MEDIA`, `DEFAULT.XBE`.
3. **PVD-velden**: `system_id` (`PLAYSTATION`, `PSP GAME`, `AMIGA`), `volume_id`, `publisher`.
4. **Root-directory-records** via PVD LBA: `PS3_DISC.SFB`, `SETUP.EXE`, `AUTORUN.INF`, `*.CAB`, `*.SH`.
5. **Begrensde tree-metadata** (`isoinfo -f`, maximaal aantal mappen): `*.EXE`, `*.INF`, `EBOOT.BIN`.
6. **Kennisgewichten** uit `knowledge.json` (begrensd door `learn.max_db_share`).
7. **Vraag-terugval** bij een lage score of gelijke stand.

## Bekende inhoudssignaturen

| Klasse | Signatuur | Voorbeeldapplicatie |
|---|---|---|
| Wii | `file`/Wii-discimage-magic | `dolphin-emu %f` |
| Xbox | `MICROSOFT*XBOX*MEDIA` + `DEFAULT.XBE` in de eerste 74 KB | `xemu %f` |
| PS1 | 2352-sectoren + `BOOT =`/`SYSTEM.CNF` | `duckstation %f` |
| PS2 | PVD `system_id=PLAYSTATION` (+ optioneel `BOOT2 =`) | `pcsx2 %f` |
| PSP | PVD `system_id=PSP GAME` | `ppsspp %f` |
| PS3 | `PS3_DISC.SFB` in root/tree | `rpcs3 %f` |
| Amiga | PVD `system_id=AMIGA` | `fs-uae %f` |
| PC/UDF | UDF-MIME, `SETUP.EXE/AUTORUN.INF/INSTALL.EXE`, `.CAB/.SH/.EXE` | `gnome-disk-image-mounter %f` |

## Bouwen

```bash
cd /mnt/VG_00/PUBLIC-LIBRARY/SRC/Own_Code/Iso-dispatcher
cmake -S . -B build -G Ninja
cmake --build build -j
./build/iso-dispatcher --help
```

Vereisten (aanwezig op het doelsysteem): Qt6 Core/Widgets 6.x, CMake, g++, ninja/make.

## CLI

```bash
iso-dispatcher                              # GUI
iso-dispatcher gui [file.iso]               # GUI met vooraf geladen bestand
iso-dispatcher classify <file.iso>          # JSON-beslissing
iso-dispatcher why <file.iso>               # volledige score/bewijs
iso-dispatcher launch <file.iso> [--class id] [--dry-run]
iso-dispatcher learn <dir> [--dry-run] [--limit N]
iso-dispatcher stats
iso-dispatcher decisions [N]
iso-dispatcher forget <fingerprint>
iso-dispatcher export <path.json>
iso-dispatcher config path|init|get|set
iso-dispatcher --help | -h | ?
```

Exitcodes: `0=ok`, `1=algemene/gebruikersfout`, `2=marker ongeldig`, `3=entry/referentie ontbreekt of ongeldig`, `4=class onbekend/niet geconfigureerd`, `5=emulator/exec ontbreekt of starten mislukt`.

## Boot markers

`iso-dispatcher` ondersteunt portable boot-markers (tekstbestanden die beginnen met `ISO-DISPATCHER-BOOT`).

Formaat:

```text
ISO-DISPATCHER-BOOT
class=ps1
entry=Game.cue
```

Validatie voor launch/classify:
- verplichte velden `class` en `entry` (of bekende auto-resolve voor PS2/PS3/PS4 layouts)
- `entry` moet relatief/veilig zijn (geen absoluut pad, geen `..` traversal)
- entry moet bestaan en leesbaar zijn
- voor `.cue` entries worden `FILE`-referenties gevalideerd
- class-alias: `psx -> ps1`

## GUI

- Bestand kiezen / naar het padveld slepen.
- `Scan`: toont het bewijs (magic, PVD, root, features) en de scoretabel.
- `Launch selected`: start de geselecteerde klasse; bij lage betrouwbaarheid wordt AskDialog getoond.
- `Remember selected for this file`: slaat de koppeling fingerprint->klasse op.

AskDialog toont het bewijs, laat de gebruiker een klasse kiezen en kan de fingerprint/signature onthouden.

## Configuratie (`$HOME/.config/iso-dispatcher.json`)

Wordt automatisch met standaardwaarden aangemaakt. Belangrijke velden:

```json
{
  "scan": {
    "fast_bytes": 65536,
    "window_bytes": 2097152,
    "pvd_record_offset": 32924,
    "root_dir_bytes": 8192,
    "max_dirs": 64,
    "use_file_probe": true
  },
  "ask": {
    "enabled": true,
    "min_score": 70,
    "tie_margin": 10,
    "headless": "menu",
    "show_evidence": true,
    "remember_file": true,
    "remember_signature": false,
    "negative_cache_hours": 24
  },
  "learn": {
    "enabled": true,
    "db": "~/.local/share/iso-dispatcher/knowledge.json",
    "alpha": 1.0,
    "half_life_days": 180,
    "min_votes": 3,
    "auto_launch_weight": 0.0,
    "override_weight": 1.5,
    "max_db_share": 0.4,
    "target_accuracy": 0.90
  },
  "fallback": { "action": "ask", "else": ["gnome-disk-image-mounter", "%f"] },
  "classes": [ /* zie standaardconfiguratie in src/core/Config.cpp */ ]
}
```

`config set` ondersteunt intern sleutels met een padachtige notatie via `/` (bijvoorbeeld `scan/window_bytes`). Gebruik in de CLI exact de sleutel zoals die in de implementatie is vastgelegd.

## Knowledge JSON

Standaardpad: `~/.local/share/iso-dispatcher/knowledge.json`.

Structuur:

```json
{
  "version": 1,
  "fingerprints": { "<fp>": "ps2" },
  "negative_cache": { "<fp>": 1760000000 },
  "weights": {
    "pvd.system_id=PLAYSTATION": {
      "ps2": { "pos": 8, "neg": 0, "updated": "..." }
    }
  },
  "decisions": [
    { "at": "...", "fingerprint": "...", "chosen": "ps2", "prompted": false, "overridden": false }
  ]
}
```

Gewicht: `log((pos + alpha) / (neg + alpha))`; alleen actief wanneer `pos+neg >= min_votes`. De bijdrage van de database wordt begrensd door `max_db_share`, zodat deze de deterministische regels niet kan overrulen.

## Modules

| Bestand | Rol |
|---|---|
| `src/core/Types.h` | Datatypes: Feature, Rule, ClassDef, ProbeResult, Decision |
| `src/core/Util.*` | Opschonen van ISO-strings, wildcardmatching, tijdstempels |
| `src/core/Config.*` | JSON-configuratie laden/opslaan, standaardwaarden, get/set |
| `src/core/IsoProbe.*` | Inhoudsgebaseerde probe: magic, PVD, root-directory-seek, begrensde token/tree-scan |
| `src/core/Classifier.*` | Regels + databasegewichten + betrouwbaarheids-/vraagbeslissing |
| `src/core/Knowledge.*` | JSON-kennisbank, fingerprint-overrides, negatieve cache, statistieken |
| `src/core/Launcher.*` | Losgekoppeld starten met `%f/%e`-vervanging; geen recursieve openers |
| `src/cli/Cli.*` | CLI-commando's, help, terminalprompt |
| `src/ui/AskDialog.*` | Qt-dialoogvenster bij onzekere classificatie |
| `src/ui/MainWindow.*` | GUI-scan/score/start/onthouden |
| `src/app/App.*` | GUI/CLI-bootstrap, initialisatie van configuratie en kennisbank |
| `src/main.cpp` | Startpunt |

## Debuggen

- `iso-dispatcher why file.iso` toont het bewijs en de uitsplitsing van de score.
- `iso-dispatcher stats` toont het kennisoverzicht.
- `iso-dispatcher decisions 20` toont recente keuzes/overrides.
- `iso-dispatcher learn <dir> --dry-run` test batchclassificatie zonder iets te leren.
- Regressietest: een Wii-image in een vreemd benoemde map moet Wii blijven; het pad mag nooit doorslaggevend zijn.

## Bekende beperkingen / volgende stappen

- `isoinfo -f` wordt gebruikt voor begrensde tree-metadata. Als `isoinfo` ontbreekt, valt de probe terug op root/PVD/token-features.
- De huidige opslag van kennis gebruikt eenvoudige JSON; bij zeer grote datasets (>50k) migreren naar SQLite.
- Uitgepakte PS3-mappen (`EBOOT.BIN`) zijn geen ISO's; later kan een aparte map-handler worden toegevoegd.
- De typeherkenning van `config set` is eenvoudig (bool/int/double/string).
- Voor distributie: test het `.desktop`-bestand en de MIME-/handlerintegratie afzonderlijk; voer nooit `xdg-open` uit op dezelfde ISO vanuit de handler.
