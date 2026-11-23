# iso-dispatcher

**Content-based ISO classifier and launcher for Linux**

A Qt6 C++ application with both a graphical interface (GUI) and command-line interface (CLI).

iso-dispatcher identifies disc image formats by inspecting their contents rather than relying on filenames or directory paths. It can classify supported platforms and launch the corresponding emulator.

When classification confidence is low, the application asks the user to select the correct platform and can remember that decision for future use.

## Features

- Content-only ISO classification (no filename or directory-name rules).
- Supports Nintendo Wii, Xbox, PlayStation 1, 2, 3, PSP and Commodore Amiga.
- PC/UDF image detection.
- GUI with drag-and-drop support.
- Command-line interface.
- Evidence-based classification with confidence scores.
- Learning system that remembers user decisions.
- Portable boot-marker support.
- Bounded scanning for efficient ISO analysis.
- Configurable emulator commands.
- JSON configuration and knowledge database.

## Supported platforms

| Platform | Default application |
|---|---|
| Nintendo Wii | Dolphin |
| Xbox | Xemu |
| PlayStation 1 | DuckStation |
| PlayStation 2 | PCSX2 |
| PlayStation Portable | PPSSPP |
| PlayStation 3 | RPCS3 |
| Commodore Amiga | FS-UAE |
| PC / UDF | Configurable image mounter |

Emulators are optional. They are only required when you want to launch the corresponding image.

---

# Installation

## 1. Dependencies

iso-dispatcher uses Qt6 and C++17.

### Required build dependencies

| Dependency | Purpose |
|---|---|
| C++17 compiler (g++) | Compile the application |
| CMake 3.16+ | Build configuration |
| Qt6 Core 6.2+ | Application framework |
| Qt6 Widgets 6.2+ | Graphical interface |
| Ninja or Make | Build system |

### Optional runtime dependencies

| Dependency | Purpose | Required |
|---|---|---|
| `file` | Detect image formats and magic signatures | Recommended |
| `isoinfo` | Inspect ISO directory metadata | Recommended |
| Qt6 runtime libraries | Run the GUI application | Yes |
| Emulator applications | Launch supported platforms | No |

**Important:** Without `isoinfo`, iso-dispatcher continues to work but has reduced directory-tree detection capabilities.

Without `file`, some format signatures cannot be detected.

---

## 2. Option A — Run precompiled binary (no build required)

Use this if you downloaded a precompiled `iso-dispatcher` binary. **No compiler, CMake, Ninja, or Qt6 development packages are needed.**

### Runtime dependencies

| Dependency | Purpose | Required |
|---|---|---|
| Qt6 Core | Application framework | Yes |
| Qt6 Widgets | Graphical interface | Yes |
| Qt6 GUI | GUI rendering | Yes |
| `file` | File format detection | Recommended |
| `isoinfo` | ISO directory inspection | Recommended |
| Emulator applications | Launch supported platforms | Optional |

Qt6 runtime libraries are normally installed automatically through the distribution package manager.

### Install runtime dependencies

#### Debian / Ubuntu / Linux Mint / Pop!_OS

```bash
sudo apt update

sudo apt install -y \
    libqt6core6 \
    libqt6widgets6 \
    libqt6gui6 \
    file \
    genisoimage
```

#### Arch Linux / CachyOS / EndeavourOS / Manjaro

```bash
sudo pacman -Syu

sudo pacman -S --needed \
    qt6-base \
    file \
    cdrtools
```

#### Fedora / Red Hat based distributions

```bash
sudo dnf install -y \
    qt6-qtbase \
    file \
    genisoimage
```

Package names and repository availability may differ on other Red Hat based distributions.

### Verify runtime dependencies

```bash
ldd ./iso-dispatcher
```

Check for missing libraries:

```bash
ldd ./iso-dispatcher | grep "not found"
```

No output means that no missing shared libraries were detected.

Check optional utilities:

```bash
command -v file
command -v isoinfo
```

### Notes

- No C++ compiler is required.
- No CMake or Ninja is required.
- No Qt6 development packages are required.
- Emulators are only needed to launch their respective platforms.
- The binary must match the target system architecture and compatible system libraries.
- For a fully portable release, consider packaging the required Qt6 libraries alongside the executable.

---

## 3. Option B — Build from source (build dependencies)

Use this if you want to compile iso-dispatcher yourself. **Build tools and Qt6 development packages are required.**

### Install build dependencies

Choose the instructions for your Linux distribution.

### Debian / Ubuntu / Linux Mint / Pop!_OS

Update your package database:

```bash
sudo apt update
```

Install dependencies:

```bash
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    qt6-base-dev \
    pkg-config \
    file \
    genisoimage
```

`genisoimage` provides the `isoinfo` utility.

### Arch Linux / CachyOS / EndeavourOS / Manjaro

Update your system:

```bash
sudo pacman -Syu
```

Install dependencies:

```bash
sudo pacman -S --needed \
    base-devel \
    cmake \
    ninja \
    qt6-base \
    pkgconf \
    file \
    cdrtools
```

`cdrtools` provides `isoinfo`.

### Fedora / Red Hat based distributions

For Fedora:

```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    ninja-build \
    qt6-qtbase-devel \
    pkgconf-pkg-config \
    file \
    genisoimage
```

For other Red Hat based distributions, package availability can differ. On RHEL-compatible systems, EPEL or additional repositories may be needed for some packages.

### Verify dependencies

Check the installed tools:

```bash
g++ --version
cmake --version
ninja --version
pkg-config --modversion Qt6Core
pkg-config --modversion Qt6Widgets
file --version
isoinfo --version
```

If `isoinfo` is unavailable, check that the appropriate ISO utility package is installed.

---

# Building from source

## 1. Clone the repository

```bash
git clone https://github.com/Mazhive/iso-dispatcher.git
cd iso-dispatcher
```

## 2. Build

The included build script automatically detects Ninja and falls back to Make if Ninja is unavailable.

```bash
chmod +x build.sh
./build.sh
```

The resulting executable will be placed in:

```text
build/iso-dispatcher
```

## 3. Run

Start the graphical application:

```bash
./build/iso-dispatcher
```

Check the CLI:

```bash
./build/iso-dispatcher --help
```

## Alternative: Manual CMake build

```bash
cmake -S . -B build -G Ninja
cmake --build build -j"$(nproc)"
```

To install system-wide:

```bash
sudo cmake --install build
```

The default installation prefix is `/usr/local`.

---

# Optional emulator installation

Emulators are independent applications and are not bundled with iso-dispatcher.

Install the emulators you want to use separately.

| Platform | Emulator | Executable |
|---|---|---|
| Wii | Dolphin | `dolphin-emu` |
| Xbox | Xemu | `xemu` |
| PS1 | DuckStation | `duckstation` |
| PS2 | PCSX2 | `pcsx2` |
| PSP | PPSSPP | `ppsspp` |
| PS3 | RPCS3 | `rpcs3` |
| Amiga | FS-UAE | `fs-uae` |

Package availability varies between distributions. Some emulators may require Flatpak, AppImage or installation from their official repositories.

Make sure the executable names match the commands configured in iso-dispatcher.

---

# Usage

## GUI

- Select an ISO file or drag it into the path field.
- Click **Scan** to inspect the image.
- Review the classification evidence and confidence score.
- Click **Launch selected** to start the configured application.
- Use **Remember selected for this file** to save a classification decision.

When classification confidence is insufficient, iso-dispatcher displays a selection dialog.

## CLI

```bash
iso-dispatcher
iso-dispatcher gui [file.iso]
iso-dispatcher classify <file.iso>
iso-dispatcher why <file.iso>
iso-dispatcher launch <file.iso> [--class id] [--dry-run]
iso-dispatcher learn <dir> [--dry-run] [--limit N]
iso-dispatcher stats
iso-dispatcher decisions [N]
iso-dispatcher forget <fingerprint>
iso-dispatcher export <path.json>
iso-dispatcher config path|init|get|set
iso-dispatcher --help
```

### Exit codes

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | General or usage error |
| 2 | Invalid boot marker |
| 3 | Missing or invalid entry/reference |
| 4 | Unknown or unconfigured class |
| 5 | Emulator missing or launch failure |

---

# Configuration

Configuration is automatically created on first launch.

Location:

```text
~/.config/iso-dispatcher.json
```

The knowledge database is stored at:

```text
~/.local/share/iso-dispatcher/knowledge.json
```

The configuration allows you to customize:

- ISO scanning limits.
- Classification confidence thresholds.
- Learning behaviour.
- Emulator executable paths.
- Emulator launch arguments.
- Fallback actions.
- Language settings.

Arguments support:

- `%f` — ISO file path.
- `%e` — Boot-marker entry path.

Example:

```json
{
  "id": "ps2",
  "label": "PlayStation 2",
  "exec": "pcsx2",
  "args": ["%f"]
}
```

---

# Boot markers

iso-dispatcher supports portable boot markers.

Example:

```text
ISO-DISPATCHER-BOOT
class=ps1
entry=Game.cue
```

Supported validation:

- Required `class` and `entry` fields.
- Relative and safe entry paths.
- Entry existence and readability.
- Validation of referenced CUE files.
- PSX alias support (`psx` → `ps1`).

## Marker maker

iso-dispatcher includes a `make-marker` CLI command to propose/create boot markers:

```bash
iso-dispatcher make-marker <path> [--class id|--system id] [--dry-run] [--force] [--json]
iso-dispatcher make-marker --batch <dir> [--class id|--system id] [--limit N] [--dry-run] [--force] [--json]
```

- `<path>` can be a game directory or a specific entry file.
- `--dry-run` shows the proposed marker without writing.
- `--force` overwrites an existing different marker (correct markers stay unchanged).
- `--json` outputs machine-readable results, including `action`, `dryRun`, `force`.
- Batch mode recursively scans directories, skips irrelevant dirs, and reports `created/overwritten/unchanged/skipped/ambiguous/errors/proposed`; exit code is non-zero when batch has ambiguous/error items.
- Deterministic entry selection: PS1/PS2 prefer `.cue`, then `.chd/.iso/.bin`; PS3 JB prefers `PS3_GAME/USRDIR/EBOOT.BIN`/`EBOOT.BIN`; PSP prefers `.iso/.cso/.pbp` or `EBOOT.PBP`.
- Ambiguous candidates are reported as an error/list (no write).

---

# Classification

iso-dispatcher uses multiple content-based detection methods:

1. Magic bytes and image structure.
2. Bounded token scanning.
3. ISO Primary Volume Descriptor (PVD).
4. Root-directory records.
5. Bounded directory-tree metadata using `isoinfo`.
6. Previously learned classification weights.
7. User selection when confidence is insufficient.

No filename or directory-name rules are used.

## Measured accuracy

Library test using 176 ISO images:

| Scan method | Accuracy |
|---|---:|
| 64 KB | 73.3% |
| 576 KB | 94.9% |
| 1 MB | 95.5% |
| 2 MB | 96.0% |
| 2 MB + PVD root seek | ~99.4% |
| + bounded tree metadata | 100% |

These are results from the tested library, not a guarantee for every ISO collection.

---

# Troubleshooting

### Qt6 not found

Make sure the Qt6 development packages are installed.

```bash
pkg-config --modversion Qt6Core
pkg-config --modversion Qt6Widgets
```

### isoinfo not found

Install the ISO utility package for your distribution.

- Debian/Ubuntu: `genisoimage`
- Arch/CachyOS: `cdrtools`
- Fedora: `genisoimage`

### Emulator does not start

Check whether the executable is available:

```bash
command -v dolphin-emu
command -v pcsx2
command -v duckstation
command -v rpcs3
```

Also verify the executable and arguments in the configuration file.

### Debugging commands

```bash
iso-dispatcher why file.iso
iso-dispatcher stats
iso-dispatcher decisions 20
iso-dispatcher learn /path/to/isos --dry-run
```

---

# License

See the repository for licensing information.

---

# Nederlandse versie

# iso-dispatcher

**Inhoudsgebaseerde ISO-classificatie en launcher voor Linux**

Een Qt6 C++-applicatie met zowel een grafische interface (GUI) als een command-line interface (CLI).

iso-dispatcher herkent ISO-bestanden door de inhoud te onderzoeken, zonder afhankelijk te zijn van bestandsnamen of mapnamen.

Bij onvoldoende herkenningszekerheid vraagt de applicatie de gebruiker om een keuze. Deze keuze kan worden onthouden voor toekomstige scans.

## Functies

- ISO-classificatie op basis van inhoud.
- Ondersteuning voor Wii, Xbox, PlayStation 1, 2, 3, PSP en Commodore Amiga.
- Detectie van PC/UDF-images.
- Grafische interface met drag-and-drop.
- Command-line interface.
- Classificatie met bewijs en betrouwbaarheidsscores.
- Leersysteem dat gebruikerskeuzes onthoudt.
- Ondersteuning voor portable boot-markers.
- Begrensde scans voor efficiënte verwerking.
- Configureerbare emulatorcommando's.
- JSON-configuratie en kennisdatabase.

## Ondersteunde platforms

| Platform | Standaardapplicatie |
|---|---|
| Nintendo Wii | Dolphin |
| Xbox | Xemu |
| PlayStation 1 | DuckStation |
| PlayStation 2 | PCSX2 |
| PlayStation Portable | PPSSPP |
| PlayStation 3 | RPCS3 |
| Commodore Amiga | FS-UAE |
| PC / UDF | Configureerbare image-mounter |

Emulators zijn optioneel en alleen nodig wanneer je de betreffende images wilt starten.

---

# Installatie

## 1. Dependencies

iso-dispatcher gebruikt Qt6 en C++17.

### Verplichte build-dependencies

| Dependency | Functie |
|---|---|
| C++17 compiler (g++) | Applicatie compileren |
| CMake 3.16+ | Buildconfiguratie |
| Qt6 Core 6.2+ | Applicatieframework |
| Qt6 Widgets 6.2+ | Grafische interface |
| Ninja of Make | Build-systeem |

### Optionele runtime-dependencies

| Dependency | Functie | Verplicht |
|---|---|---|
| `file` | Herkennen van imageformaten en signatures | Aanbevolen |
| `isoinfo` | ISO-directorymetadata uitlezen | Aanbevolen |
| Qt6 runtime libraries | GUI-applicatie uitvoeren | Ja |
| Emulatorapplicaties | Ondersteunde platforms starten | Nee |

**Belangrijk:** Zonder `isoinfo` blijft iso-dispatcher werken, maar is de detectie van directorystructuren beperkter.

Zonder `file` kunnen bepaalde formatsignatures niet worden herkend.

## 2. Optie A — Voorgecompileerde binary uitvoeren (geen build nodig)

Gebruik dit als je een voorgecompileerde `iso-dispatcher` binary hebt gedownload. **Geen compiler, CMake, Ninja of Qt6-developmentpakketten nodig.**

### Runtime-dependencies

| Dependency | Functie | Vereist |
|---|---|---|
| Qt6 Core | Applicatieframework | Ja |
| Qt6 Widgets | Grafische interface | Ja |
| Qt6 GUI | GUI-weergave | Ja |
| `file` | Bestandsformaatdetectie | Aanbevolen |
| `isoinfo` | ISO-directory inspectie | Aanbevolen |
| Emulatorapplicaties | Ondersteunde platforms starten | Optioneel |

Qt6-runtimebibliotheken worden normaal gesproken automatisch geïnstalleerd via de pakketbeheerder van de distributie.

### Runtime-dependencies installeren

#### Debian / Ubuntu / Linux Mint / Pop!_OS

```bash
sudo apt update

sudo apt install -y \
    libqt6core6 \
    libqt6widgets6 \
    libqt6gui6 \
    file \
    genisoimage
```

#### Arch Linux / CachyOS / EndeavourOS / Manjaro

```bash
sudo pacman -Syu

sudo pacman -S --needed \
    qt6-base \
    file \
    cdrtools
```

#### Fedora / Red Hat gebaseerde distributies

```bash
sudo dnf install -y \
    qt6-qtbase \
    file \
    genisoimage
```

Pakketnamen en beschikbaarheid kunnen verschillen bij andere Red Hat gebaseerde distributies.

### Runtime-dependencies controleren

Controleer de gedeelde bibliotheken:

```bash
ldd ./iso-dispatcher
```

Controleer op ontbrekende bibliotheken:

```bash
ldd ./iso-dispatcher | grep "not found"
```

Geen uitvoer betekent dat er geen ontbrekende gedeelde bibliotheken zijn gevonden.

Controleer de optionele programma's:

```bash
command -v file
command -v isoinfo
```

### Opmerkingen

- Geen C++-compiler nodig.
- Geen CMake of Ninja nodig.
- Geen Qt6-developmentpakketten nodig.
- Emulators zijn alleen nodig om hun betreffende platforms te starten.
- De binary moet overeenkomen met de architectuur en compatibele systeembibliotheken van het doelsysteem.
- Voor een volledig portable release kun je overwegen de benodigde Qt6-bibliotheken samen met het uitvoerbare bestand te distribueren.

---

## 3. Optie B — Zelf compileren (build-dependencies)

Gebruik dit als je iso-dispatcher zelf wilt bouwen. **Build-tools en Qt6-developmentpakketten zijn vereist.**

### Build-dependencies installeren

Kies de instructies voor jouw Linux-distributie.

### Debian / Ubuntu / Linux Mint / Pop!_OS

```bash
sudo apt update

sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    qt6-base-dev \
    pkg-config \
    file \
    genisoimage
```

`genisoimage` levert het programma `isoinfo`.

### Arch Linux / CachyOS / EndeavourOS / Manjaro

```bash
sudo pacman -Syu

sudo pacman -S --needed \
    base-devel \
    cmake \
    ninja \
    qt6-base \
    pkgconf \
    file \
    cdrtools
```

`cdrtools` levert het programma `isoinfo`.

### Fedora / Red Hat gebaseerde distributies

Voor Fedora:

```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    ninja-build \
    qt6-qtbase-devel \
    pkgconf-pkg-config \
    file \
    genisoimage
```

Voor andere Red Hat gebaseerde distributies kunnen de pakketnamen verschillen. Op RHEL-compatibele systemen kunnen EPEL of aanvullende repositories nodig zijn.

### Dependencies controleren

```bash
g++ --version
cmake --version
ninja --version
pkg-config --modversion Qt6Core
pkg-config --modversion Qt6Widgets
file --version
isoinfo --version
```

---

# Compileren vanuit de broncode

## 1. Repository downloaden

```bash
git clone https://github.com/Mazhive/iso-dispatcher.git
cd iso-dispatcher
```

## 2. Compileren

```bash
chmod +x build.sh
./build.sh
```

Het buildscript detecteert automatisch Ninja en gebruikt anders Make.

De uitvoerbare applicatie wordt geplaatst in:

```text
build/iso-dispatcher
```

## 3. Starten

Grafische interface:

```bash
./build/iso-dispatcher
```

CLI-help:

```bash
./build/iso-dispatcher --help
```

## Alternatief: Handmatig compileren met CMake

```bash
cmake -S . -B build -G Ninja
cmake --build build -j"$(nproc)"
```

Systeemwijd installeren:

```bash
sudo cmake --install build
```

De standaardinstallatielocatie is `/usr/local`.

---

# Optionele emulatorinstallatie

Emulators zijn afzonderlijke applicaties en worden niet meegeleverd met iso-dispatcher.

| Platform | Emulator | Uitvoerbaar bestand |
|---|---|---|
| Wii | Dolphin | `dolphin-emu` |
| Xbox | Xemu | `xemu` |
| PS1 | DuckStation | `duckstation` |
| PS2 | PCSX2 | `pcsx2` |
| PSP | PPSSPP | `ppsspp` |
| PS3 | RPCS3 | `rpcs3` |
| Amiga | FS-UAE | `fs-uae` |

Installeer alleen de emulators die je daadwerkelijk wilt gebruiken.

De beschikbaarheid verschilt per distributie. Sommige emulators vereisen Flatpak, AppImage of installatie vanuit hun officiële repositories.

---

# Gebruik

## GUI

- Selecteer een ISO-bestand of sleep het naar het padveld.
- Klik op **Scan** om het image te onderzoeken.
- Bekijk de herkenningsresultaten en betrouwbaarheidsscore.
- Klik op **Launch selected** om de applicatie te starten.
- Gebruik **Remember selected for this file** om een keuze te onthouden.

Bij onvoldoende herkenningszekerheid verschijnt een selectiedialoog.

## CLI

```bash
iso-dispatcher
iso-dispatcher gui [file.iso]
iso-dispatcher classify <file.iso>
iso-dispatcher why <file.iso>
iso-dispatcher launch <file.iso> [--class id] [--dry-run]
iso-dispatcher learn <dir> [--dry-run] [--limit N]
iso-dispatcher stats
iso-dispatcher decisions [N]
iso-dispatcher forget <fingerprint>
iso-dispatcher export <path.json>
iso-dispatcher config path|init|get|set
iso-dispatcher --help
```

---

# Configuratie

De configuratie wordt automatisch aangemaakt bij de eerste start.

Configuratiebestand:

```text
~/.config/iso-dispatcher.json
```

Kennisdatabase:

```text
~/.local/share/iso-dispatcher/knowledge.json
```

Je kunt onder andere het volgende configureren:

- ISO-scanlimieten.
- Betrouwbaarheidsdrempels.
- Leergedrag.
- Emulatorpaden.
- Emulatorargumenten.
- Terugvalacties.
- Taalinstellingen.

Argumenten ondersteunen:

- `%f` — Pad naar het ISO-bestand.
- `%e` — Pad naar de boot-marker-entry.

---

# Boot markers

iso-dispatcher ondersteunt portable boot-markers.

Voorbeeld:

```text
ISO-DISPATCHER-BOOT
class=ps1
entry=Game.cue
```

De applicatie controleert:

- Verplichte velden `class` en `entry`.
- Veilige relatieve paden.
- Bestaan en leesbaarheid van de entry.
- Verwijzingen naar bestanden in CUE-bestanden.
- PSX-alias (`psx` → `ps1`).

## Marker-maker

iso-dispatcher bevat een `make-marker` CLI-commando om boot-markers voor te stellen/aan te maken:

```bash
iso-dispatcher make-marker <pad> [--class id|--system id] [--dry-run] [--force] [--json]
iso-dispatcher make-marker --batch <map> [--class id|--system id] [--limit N] [--dry-run] [--force] [--json]
```

- `<pad>` kan een gamemap of een specifiek entry-bestand zijn.
- `--dry-run` toont de voorgestelde marker zonder te schrijven.
- `--force` overschrijft een bestaande afwijkende marker (correcte markers blijven `unchanged`).
- `--json` geeft machine-leesbare output, inclusief `action`, `dryRun`, `force`.
- Batch-modus scant recursief mappen, slaat irrelevante mappen over en rapporteert `created/overwritten/unchanged/skipped/ambiguous/errors/proposed`; exit code is niet-0 bij ambigu/error-items.
- Deterministische entry-selectie: PS1/PS2 voorkeur `.cue`, daarna `.chd/.iso/.bin`; PS3 JB voorkeur `PS3_GAME/USRDIR/EBOOT.BIN`/`EBOOT.BIN`; PSP voorkeur `.iso/.cso/.pbp` of `EBOOT.PBP`.
- Meerdere gelijkwaardige kandidaten geven een ambigu-fout/lijst (geen write).

---

# Classificatie

iso-dispatcher gebruikt meerdere detectiemethoden:

1. Magic bytes en bestandsstructuur.
2. Begrensde token-scans.
3. ISO Primary Volume Descriptor (PVD).
4. Root-directory-records.
5. Begrensde directory-tree-metadata via `isoinfo`.
6. Eerder geleerde classificatiegewichten.
7. Gebruikerskeuze bij onvoldoende zekerheid.

Bestandsnamen en mapnamen worden nooit gebruikt voor classificatie.

## Gemeten nauwkeurigheid

Test met een bibliotheek van 176 ISO-images:

| Scanmethode | Nauwkeurigheid |
|---|---:|
| 64 KB | 73,3% |
| 576 KB | 94,9% |
| 1 MB | 95,5% |
| 2 MB | 96,0% |
| 2 MB + PVD root seek | ~99,4% |
| + begrensde tree-metadata | 100% |

Dit zijn testresultaten van de gebruikte bibliotheek, geen garantie voor iedere ISO-collectie.

---

# Problemen oplossen

### Qt6 niet gevonden

Controleer of de Qt6-developmentpakketten zijn geïnstalleerd.

```bash
pkg-config --modversion Qt6Core
pkg-config --modversion Qt6Widgets
```

### isoinfo niet gevonden

Installeer het ISO-utilitypakket voor jouw distributie:

- Debian/Ubuntu: `genisoimage`
- Arch/CachyOS: `cdrtools`
- Fedora: `genisoimage`

### Emulator start niet

Controleer of het uitvoerbare bestand beschikbaar is:

```bash
command -v dolphin-emu
command -v pcsx2
command -v duckstation
command -v rpcs3
```

Controleer ook de emulatorinstellingen en argumenten in het configuratiebestand.

### Debugcommando's

```bash
iso-dispatcher why file.iso
iso-dispatcher stats
iso-dispatcher decisions 20
iso-dispatcher learn /path/to/isos --dry-run
```

---

# Licentie

Zie de repository voor informatie over de licentie.
