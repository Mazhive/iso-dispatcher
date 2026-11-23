# iso-dispatcher

Content-only ISO classifier and launcher (Qt6 C++ GUI + CLI).

## Doel

Bepaal het platform/type van een `.iso` **uitsluitend op basis van ISO-inhoud** (magic bytes, PVD, root-directory records, bounded token scan). Er worden **geen pad- of mapnaamregels** gebruikt voor classificatie. Bij lage zekerheid vraagt de app de gebruiker, en slaat de keuze op in een kennisbestand voor volgende keren.

## Belangrijkste ontwerpregels

- **Geen pad/folder-classificatie**: de locatie op schijf mag niet meebeslissen.
- **Bounded reads**: geen volledige ISO scan. Standaard: 64 KB fast header + PVD + root-dir seek + 2 MB token window + beperkte tree-metadata.
- **Geen extra apt install nodig**: Qt6 Core/Widgets + CMake + g++.
- **Config**: `$HOME/.config/iso-dispatcher.json` (automatisch aangemaakt bij eerste run).
- **Kennis**: JSON, in-memory `QHash`, batch-save. SQLite is optioneel toekomstpad.
- **Veiligheid**: launcher weigert `xdg-open`/`gio open`/`kde-open` om recursie te voorkomen.

## Gemeten nauwkeurigheid (bibliotheektest, 176 ISO's)

Metingen op `pad:/`:

| Scan-window | Correct | Accuracy | Opmerking |
|---|---:|---:|---|
| 64 KB | 129/176 | 73.3% | Console-formaten meestal al klaar |
| 576 KB | 167/176 | 94.9% | 90%-doel bereikt |
| 1 MB | 168/176 | 95.5% | |
| 2 MB | 169/176 | 96.0% | |
| 2 MB + PVD root-dir seek | ~175/176 | ~99.4% | Geen padregels nodig |
| + bounded tree-walk metadata | 176/176 | 100% | Content-only |

Belangrijk: de overblijvende "stugge" ISO's hadden markers buiten 2 MB, maar de PVD root-directory pointer wees er rechtstreeks naar. Daarom gebruikt de probe een **PVD root-seek** i.p.v. lineair verder lezen.

## Classificatieketen

Volgorde (eerste sterke signalen wegen zwaarst):

1. **Magic / file-structuur**: Wii magic `5D1C9EA3@0x1C`, 2352-sector PS1, UDF.
2. **Raw token scan** (bounded): `MICROSOFT*XBOX*MEDIA`, `DEFAULT.XBE`.
3. **PVD velden**: `system_id` (`PLAYSTATION`, `PSP GAME`, `AMIGA`), `volume_id`, `publisher`.
4. **Root-directory records** via PVD LBA: `PS3_DISC.SFB`, `SETUP.EXE`, `AUTORUN.INF`, `*.CAB`, `*.SH`.
5. **Bounded tree-metadata** (`isoinfo -f`, max dirs): `*.EXE`, `*.INF`, `EBOOT.BIN`.
6. **Kennisgewichten** uit `knowledge.json` (capped door `learn.max_db_share`).
7. **Ask fallback** bij lage score/tie.

## Bekende inhouds-signaturen

| Klasse | Signatuur | Voorbeeld-app |
|---|---|---|
| Wii | `file`/magic Wii disc image | `dolphin-emu %f` |
| Xbox | `MICROSOFT*XBOX*MEDIA` + `DEFAULT.XBE` in eerste 74 KB | `xemu %f` |
| PS1 | 2352 sectors + `BOOT =`/`SYSTEM.CNF` | `duckstation %f` |
| PS2 | PVD `system_id=PLAYSTATION` (+ optioneel `BOOT2 =`) | `pcsx2 %f` |
| PSP | PVD `system_id=PSP GAME` | `ppsspp %f` |
| PS3 | `PS3_DISC.SFB` in root/tree | `rpcs3 %f` |
| Amiga | PVD `system_id=AMIGA` | `fs-uae %f` |
| PC/UDF | UDF mime, `SETUP.EXE/AUTORUN.INF/INSTALL.EXE`, `.CAB/.SH/.EXE` | `gnome-disk-image-mounter %f` |

## Build

```bash
cd /mnt/VG_00/PUBLIC-LIBRARY/SRC/Own_Code/Iso-dispatcher
cmake -S . -B build -G Ninja
cmake --build build -j
./build/iso-dispatcher --help
```

Vereisten (aanwezig op doel-systeem): Qt6 Core/Widgets 6.x, CMake, g++, ninja/make.

## CLI

```bash
iso-dispatcher                              # GUI
iso-dispatcher gui [file.iso]               # GUI met preload
iso-dispatcher classify <file.iso>          # JSON beslissing
iso-dispatcher why <file.iso>               # volledige score/evidence
iso-dispatcher launch <file.iso> [--class id] [--dry-run]
iso-dispatcher learn <dir> [--dry-run] [--limit N]
iso-dispatcher stats
iso-dispatcher decisions [N]
iso-dispatcher forget <fingerprint>
iso-dispatcher export <path.json>
iso-dispatcher config path|init|get|set
iso-dispatcher --help | -h | ?
```

Exit codes: `0=ok`, `2=bad args`, `3=headless ambiguous abort`, `4=cancelled`, `5=launch failed`.

## GUI

- Bestand kiezen / slepen in padveld.
- `Scan`: toont evidence (magic, PVD, root, features) en scoretabel.
- `Launch selected`: start geselecteerde klasse; bij lage confidence toont AskDialog.
- `Remember selected for this file`: slaat fingerprint->klasse op.

AskDialog toont bewijs, laat klasse kiezen, en kan fingerprint/signature onthouden.

## Config (`$HOME/.config/iso-dispatcher.json`)

Wordt automatisch aangemaakt met defaults. Belangrijke velden:

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
  "classes": [ /* zie default config in src/core/Config.cpp */ ]
}
```

`config set` ondersteunt dot-achtige keys via `/` in interne implementatie (`scan/window_bytes`). Gebruik in CLI de exacte key zoals geïmplementeerd.

## Knowledge JSON

Default pad: `~/.local/share/iso-dispatcher/knowledge.json`.

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
  "decisions": [ { "at": "...", "fingerprint": "...", "chosen": "ps2", "prompted": false, "overridden": false } ]
}
```

Gewicht: `log((pos + alpha) / (neg + alpha))`, alleen actief bij `pos+neg >= min_votes`. DB-bijdrage wordt gecapped door `max_db_share` om deterministische regels niet te overrulen.

## Modules

| Bestand | Rol |
|---|---|
| `src/core/Types.h` | Datatypes: Feature, Rule, ClassDef, ProbeResult, Decision |
| `src/core/Util.*` | ISO string cleanup, wildcard, timestamps |
| `src/core/Config.*` | JSON config laden/saven/defaults/get/set |
| `src/core/IsoProbe.*` | Content-only probe: magic, PVD, root-dir seek, bounded token/tree scan |
| `src/core/Classifier.*` | Regels + DB-gewichten + confidence/ask-beslissing |
| `src/core/Knowledge.*` | JSON kennisbank, fingerprint overrides, negatieve cache, stats |
| `src/core/Launcher.*` | Detached launch met `%f/%e` substitutie, geen recursive openers |
| `src/cli/Cli.*` | CLI commandos, help, terminal prompt |
| `src/ui/AskDialog.*` | Qt dialog bij onzekerheid |
| `src/ui/MainWindow.*` | GUI scan/score/launch/remember |
| `src/app/App.*` | GUI/CLI bootstrap, config/knowledge init |
| `src/main.cpp` | Entrypoint |

## Debugging

- `iso-dispatcher why file.iso` geeft evidence + score-afbraak.
- `iso-dispatcher stats` geeft kennisoverzicht.
- `iso-dispatcher decisions 20` toont recente keuzes/overrides.
- `iso-dispatcher learn <dir> --dry-run` test batch-classificatie zonder te leren.
- Regressie: Wii-image in vreemde map moet Wii blijven; pad mag nooit winnen.

## Bekende beperkingen / volgende stappen

- `isoinfo -f` wordt gebruikt voor bounded tree-metadata; als `isoinfo` ontbreekt, valt de probe terug op root/PVD/token features.
- De huidige knowledge-write is eenvoudig JSON; bij zeer grote datasets (>50k) naar SQLite migreren.
- PS3 uitgepakte mappen (`EBOOT.BIN`) zijn geen ISO; aparte map-handler kan later.
- `config set` type-detectie is simpel (bool/int/double/string).
- Voor distributie: `.desktop` bestand en MIME/handler integratie afzonderlijk testen; nooit `xdg-open` op dezelfde ISO vanuit handler.
