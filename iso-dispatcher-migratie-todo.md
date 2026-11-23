# ISO-dispatcher + game-gui-fork migratie TODO

> Werk stap voor stap. Ga pas door als de test van de huidige stap slaagt.
> Noteer per stap: gewijzigd bestand, testcommando, verwacht resultaat, werkelijk resultaat.

## Status (live)
- [x] Todo-checklist aangemaakt.
- [ ] Ingebouwde OpenCode Todos-sidebar kan ik niet rechtstreeks vullen (geen tool beschikbaar); gebruik dit bestand als tracker.
- [x] Fase A afgerond in `/mnt/VG_00/PUBLIC-LIBRARY/SRC/Own_Code/Iso-dispatcher` (build OK, `build/iso-dispatcher`):
  - [x] A1-A3: marker-preflight, veilige entry, ontbrekende entry, cue FILE-validatie.
  - [x] A4: `psx -> ps1` alias/normalisatie.
  - [x] A5: exit codes/preflight (0/1/2/3/4/5).
  - [x] A6: `launch --dry-run` resolutie/preflight zonder emulatorstart.
  - [x] A7: help/README bijgewerkt.
- [x] Regressie boot-markers (sample 19/149 gevonden markers): 7x PS3 + 12x PS1 `classify` → alle OK, class/bootEntry correct. Geen `.ps2boot.iso`/`.pspboot.iso` markers gevonden in bibliotheek.
- [x] B1 marker-maker ontwerp/scope + dry-run geïmplementeerd (`make-marker` subcommand, `MarkerMaker` core class).
- [x] B2 entry-selectieregels per systeem geverifieerd/werkend.
- [x] B3 idempotent schrijven + rapportage geïmplementeerd (`--force`, created/unchanged/skipped/overwritten, JSON action).
- [x] B4 batch-modus geïmplementeerd: `make-marker --batch <dir>` recursief, `--limit`, `--dry-run/--force/--json`, rapport/counts `created/overwritten/unchanged/skipped/ambiguous/errors/proposed`; exit 1 bij ambigu/error.
- [x] **Fase B afgerond.** Volgende: Fase C `emu-launch.sh` adapter (C1-C6).

## Fase A — `iso-dispatcher` robuust maken (contract eerst)

### A1. Marker-formaat valideren (preflight)
- [x] Wijziging: marker-preflight toegevoegd voor boot-markers (`IsoProbe.cpp`, `Types.h`).
- [x] Checks: header `ISO-DISPATCHER-BOOT`; verplichte velden `class=` en `entry=`; duidelijke fout bij ontbrekend/ongeldig veld.
- [x] Test 1 (geldig): Ace Combat 2 `.ps1boot.iso` classify → class `ps1`, exit 0.
- [x] Test 2 (ongeldig): temp marker zonder `entry=` + `launch --dry-run` → exit 2, duidelijke fout.

### A2. Entry-pad veilig en aanwezig controleren
- [x] Wijziging: resolve `entry` relatief aan marker-map; absolute paden en `..` traversal geblokkeerd.
- [x] Checks: bestaat/leesbaar; onderscheid niet gevonden vs niet leesbaar.
- [x] Test 1 (ontbrekend): `entry=missing.cue` → exit 3, “Referentie niet gevonden...”.
- [x] Test 2 (traversal): `entry=../../etc/passwd` → exit 2, “Ongeldig/onveilig entry-pad”.
- [x] Test 3 (bestaand): geldige marker/Ace Combat entry → preflight OK.

### A3. `.cue` referentie-validatie
- [x] Wijziging: cue `FILE`-referenties gevalideerd relatief aan cue-map (`IsoProbe.cpp`).
- [x] Checks: gequote/ongequote FILE, exact ontbrekend bestand gerapporteerd.
- [x] Test 1 (kapotte cue): temp cue met ontbrekende `FILE "nope.bin"` → exit 3, duidelijke fout.
- [x] Test 2 (goede cue): Ace Combat 2 cue na fix → geen cue-referentiefout.
- [x] Test 3 (multi-track): Mickey's Wild Adventure cue — `launch --dry-run` geen cue-referentiefout (classificatie was onzeker, maar cue refs OK).

### A4. Class-alias/normalisatie (`psx` -> `ps1`)
- [x] Wijziging: `psx` alias naar `ps1` in Config/Classifier/Launcher/CLI.
- [x] Checks: genormaliseerde class gebruikt; bestaande `ps1` werkt.
- [x] Test 1: marker met `class=psx` + relatieve entry → exit 0, resolved als `ps1`.
- [x] Test 2: `launch <ps1boot.iso> --class psx --dry-run` → exit 0, `Chosen class: ps1`.
- [x] Test 3: bestaande `class=ps1` markers — geen regressie gezien.

### A5. Deterministische CLI/exit codes
- [x] Wijziging: exit codes/preflight in CLI toegevoegd (0/1/2/3/4/5).
- [x] Exit codes: `0` succes; `2` marker ongeldig; `3` entry/referentie ontbreekt of ongeldig; `4` class onbekend/niet geconfigureerd; `5` emulator/exec ontbreekt; `1` algemene fout.
- [x] Test 1: geldige dry-run → exit `0`.
- [x] Test 2: ontbrekende entry → exit `3`.
- [x] Test 3: onbekende class → exit `4`.
- [x] Test 4: class zonder bestaand exec → exit `5`.
- [x] Test 5: stderr bevat duidelijke foutcategorie/boodschap; parsebaar voor scripts.

### A6. `dry-run` en launch-gedrag aanscherpen
- [x] Wijziging: `launch --dry-run` doet preflight + class/exec-resolutie zonder emulator te starten.
- [x] Output: toont chosen class, boot marker, resolved entry, exec, args.
- [x] Test 1: geldige marker `--class ps1 --dry-run` → exit 0, resolutie getoond, geen launch.
- [x] Test 2: ongeldige marker met `--dry-run` → dezelfde validatiefout/exit code, geen launch.

### A7. Documentatie/config-contract bijwerken
- [x] Wijziging: help/README bijgewerkt voor marker-formaat, class-aliases, exit codes, dry-run, cue-validatie.
- [x] Test: `iso-dispatcher --help` toont Boot markers + exit codes; README bijgewerkt. Build OK.

## Fase B — Marker-maker (input-normalisatie)

### B1. Marker-maker ontwerp/scope bepalen
- [x] Wijziging: toolvorm gekozen als onderdeel van iso-dispatcher: `make-marker` subcommand (ook `makemarker`/`make_marker`). Core-logica in `src/core/MarkerMaker.{h,cpp}` voor hergebruik in B2-B4.
- [x] Contract: input = map of bestand + optionele `--class/--system`; output = voorgestelde `.psXboot.iso` marker of duidelijke ambigu/error. Flags: `--dry-run`, `--json`, `--class/--system`. Statussen: `proposed|unchanged|exists|ambiguous|error`.
- [x] Test: `make-marker "/.../PSX/Ace_Combat_2" --class ps1 --dry-run` → status `proposed`, entry `Ace_Combat_2_[SCES-00699].cue`, marker `Ace_Combat_2.ps1boot.iso`, content getoond, `(dry-run) not writing`, exit 0. Zonder class: duidelijke fout "Provide --class <id>".

### B2. Entry-selectieregels per systeem
- [x] Wijziging: deterministische entry-selectie in `MarkerMaker` geverifieerd (PS1/PS2 cue-first, daarna chd/iso/bin; PS3 bekende EBOOT-paden; PSP iso/cso/pbp/EBOOT.PBP; generiek bestaande relatieve paden; boot-markers uitgesloten).
- [x] Regels: PS1/PS2 voorkeur `.cue` dan `.chd/.iso/.bin` (voorzichtig); PS3 JB `EBOOT.BIN`/`PS3_GAME/USRDIR/EBOOT.BIN`; PSP `.iso/.cso/.pbp` of `EBOOT.PBP`; generiek alleen bestaande relatieve paden.
- [x] Test 1: Ace Combat 2 map + `--class ps1 --dry-run` → kiest `.cue`, status proposed, geen write.
- [x] Test 2: PS3 JB map `BLES00876-Toy Story 3` + `--class ps3 --dry-run` → kiest `PS3_GAME/USRDIR/EBOOT.BIN` (status unchanged, marker al up-to-date). Fresh temp PS3 JB → proposed met zelfde EBOOT-entry.
- [x] Test 3: temp dir met `GameA.cue` + `GameB.cue` → status `ambiguous`, candidates-lijst, geen write.

### B3. Idempotent schrijven + rapportage
- [x] Wijziging: `--dry-run`, `--force`, leesbare rapportage/JSON. Bestaande correcte marker niet overschrijven; bestaande andere marker zonder `--force` skip+warning; met `--force` overwrite met duidelijke logging. JSON bevat `action`, `dryRun`, `force`.
- [x] Test 1: zelfde temp map twee keer `make-marker --class ps1` → eerste `Created`, tweede `unchanged` + `Skipped: marker already up-to-date`.
- [x] Test 2: bestaande andere marker zonder `--force` → `Warning: marker exists with different content; use --force...`, exit 0, inhoud ongewijzigd.
- [x] Test 3: met `--force` → `Overwritten: ...`, inhoud correct vervangen. `--force --dry-run --json` rapporteert `action=overwrite`, `dryRun=true`.

### B4. Batch-modus voor bibliotheek
- [x] Wijziging: `make-marker --batch <dir>` verwerkt directory-recursief met `--limit`, `--dry-run`, `--force`, `--json`; rapporteert created/overwritten/unchanged/skipped/ambiguous/errors/proposed.
- [x] Test: kleine temp testsubset PSX-mappen — rapport correct (created 2, ambiguous 1, errors 0), rerun unchanged 2, limit=1 verwerkt 1; geen crash; exit code 1 bij ambigu/error.

## Fase C — `emu-launch.sh` herschrijven naar iso-dispatcher-adapter

**Bestand:** `/mnt/VG_00/PUBLIC-LIBRARY/PRE-INSTALLED-GAMES/WINEPREFIX/template-scripts/game-launchers/EMU-LAUNCHERS/emu-launch.sh`

### C1. Contract behouden, adapterlaag toevoegen
- [x] Wijziging: behoud env-contract `GAME_SYSTEM`, `GAME_ROM`, `GAME_DISPLAY_NAME`, `GAME_EMU_CONF`. Voeg `ISO_DISPATCHER_BIN` override toe (workflow-local in `emu-launch.sh`, niet naar GitHub).
- [x] Test: `bash -n` OK; met geldige env-vars + ontbrekende conf geeft `[emu] FOUT: emulation.conf niet gevonden...`, geen missing-env regressie.

### C2. Systeem-mapping `fork -> iso-dispatcher class`
- [x] Wijziging: expliciete mapping in `emu-launch.sh` (`psx->ps1`, anders pass-through), `ISO_CLASS` gezet (workflow-local).
- [x] Test mapping: `psx->ps1`, `ps2->ps2`, `psp->psp` geverifieerd. End-to-end `--class` doorvoer wordt in C4 afgemaakt.

### C3. Folder-dump pre-processing behouden (tijdelijk)
- [x] Wijziging: bestaande map-pre-processing behouden; boot-marker voorkeur toegevoegd in `_rom_in_dir` (`*.ps1/ps2/ps3/psp/psxboot.iso` eerst). Workflow-local.
- [x] Test 1: map met `.cue/.bin` aanwezig → rom-in-map gekozen, geen xdvdfs.
- [x] Test 2: xdvdfs aanwezig op systeem; bestaande pack/logica ongewijzigd (niet end-to-end met echte Xbox-dump getest).
- [x] Test 3: map met `.ps1boot.iso` + andere `.iso` → boot marker gekozen.

### C4. iso-dispatcher-first launch met fallback
- [x] Wijziging: ISO/disc-kandidaten (`ps1/ps2/ps3/psp/gamecube/wii/wiiu/xbox/xbox360/dvd` + disc/marker extensies) gaan eerst via `iso-dispatcher launch <resolved> --class <ISO_CLASS>`; bij ontbrekende dispatcher of non-zero exit: duidelijke log + fallback naar emulation.conf-pad. Workflow-local.
- [x] Test 1: PSX marker + fake dispatcher → `launch --class ps1` aangeroepen, exit 0.
- [x] Test 2: SNES `.sfc` → geen iso-dispatcher poging, fallback naar oude emu-pad.
- [x] Test 3: ontbrekende dispatcher → `[emu] iso-dispatcher niet beschikbaar... fallback...`, geen crash.

### C5. Preconf/install gedrag herplaatsen of behouden
- [x] Wijziging: install/preconf/emu-checks verplaatst naar fallback-pad; iso-dispatcher-succespad exit vroeg, geen onnodige install/preconf side effects. Workflow-local.
- [x] Test 1: ontbrekende emulator in fallback-pad → `emu-install.sh` poging + duidelijke fout, exit 1.
- [x] Test 2: ISO-systeem via iso-dispatcher (succes) met missing emu in conf → geen install/preconf, exit 0.
- [x] Test 3: `_apply_preconf` gebruikt nog steeds `cp -rn` (code ongewijzigd).

### C6. Logging/exit codes normaliseren
- [x] Wijziging: iso-dispatcher stdout/stderr pass-through; `[emu]`/`[game]` prefixen behouden; iso-dispatcher exit code gelogd bij fallback.
- [x] Test: CLI/shell-verificatie OK (duidelijke logs, exitcodes correct). End-to-end game-gui-fork terminaltab nog niet handmatig geverifieerd.

## Fase D — `game-gui-fork.py` en gerelateerde files aanpassen

**Bestanden:** `game-gui-fork.py`, `EMU-LAUNCHERS/emulation.conf`, `~/.config/gamelauncher/emulators.json`, `README-game-gui-fork.md`, `gui-version/changelog.md`

### D1. `_start_emu` geschikt maken voor iso-dispatcher flow
- [x] Wijziging: ISO/disc-systemen (`psx/ps1/ps2/ps3/psp/gamecube/wii/wiiu/xbox/xbox360/dvd`) gaan via één leidend pad: `emu-launch.sh` (die iso-dispatcher-first doet). `psx->ps1` mapping zit in emu-launch.sh.
- [x] Wijziging: ontbrekende emu in conf blokkeert ISO-first systemen niet meer in GUI; emu-launch.sh handelt af/fallback.
- [ ] Test 1: klik PSX game/marker in fork → emulator start (handmatige GUI-smoke nog nodig).
- [ ] Test 2: PS2/PSP/Wii/Xbox smoke test via fork (handmatig).
- [x] Test 3: "draait al/focus" gedrag ongewijzigd in code.

### D2. Ontbrekende-emulator afhandeling aanpassen
- [x] Wijziging: GUI toont specifiekere hint bij iso-dispatcher/exec-missing failures (exit 5/log-detectie); fallback/install blijft via emu-launch.sh/conf-pad.
- [ ] Test: ongeldige exec in iso-dispatcher config voor testclass → duidelijke GUI-melding (handmatige GUI-smoke nog nodig).

### D3. Config-afhankelijkheden reduceren
- [x] Wijziging: `emu-launch.sh` waarschuwt maar faalt niet hard bij ontbrekende conf/sectie voor ISO-dispatcher classes; ISO-first kan door. GUI blokkeert ISO-first niet op ontbrekende emu in conf.
- [x] Wijziging: conf/fallback behouden voor niet-ISO systemen.
- [x] Test 1: PSX marker zonder conf-file + fake dispatcher → iso-dispatcher-first succes, exit 0.
- [x] Test 2: niet-ISO/fallback pad behouden (eerder C4/C5 geverifieerd).

### D4. Marker-first game-detectie
- [x] Wijziging: scanner filtert bijbehorende payload-bestanden weg wanneer boot markers aanwezig zijn (`_prefer_boot_markers`); boot markers worden als startbare items behouden.
- [ ] Test 1: map met marker + cue/bin toont één startbaar item (code-logica toegevoegd; handmatige GUI-scan nog nodig).
- [ ] Test 2: marker launch gebruikt entry uit marker correct (door iso-dispatcher/emu-launch.sh pad; end-to-end GUI smoke nog nodig).

### D5. Metadata/titels/icons zonder emulation.conf
- [x] Wijziging: lichte `_SYSTEM_TITLES` mapping toegevoegd voor ISO/disc systemen; scanner gebruikt conf title of system-mapping fallback.
- [ ] Test: nav toont correcte platform/systeemtitels/icons bij afwezigheid van ISO-conf entries (handmatige GUI-check).

### D6. Docs/changelog bijwerken
- [x] Wijziging: `README-game-gui-fork.md` en `gui-version/changelog.md` bijgewerkt met iso-dispatcher-first flow, `psx->ps1`, `ISO_DISPATCHER_BIN`, fallback/conf-tolerantie, marker-first scan.
- [x] Test: leescheck OK (nieuwe flow beschreven).

## Fase E — Opschoning en regressie

### E1. Uitfaseren `emulation.conf`/`emulators.json` voor ISO-systems
- [ ] Wijziging: verwijder/markeer ISO-systeemsecties als deprecated alleen na succesvolle migratie; behoud niet-ISO secties.
- [ ] Test: `grep -n "emulation.conf\|emulators.json" game-gui-fork.py emu-launch.sh` — verwacht: geen verplichte afhankelijkheid meer voor ISO-systems.

### E2. Regressietestmatrix
- [ ] PS1 `.ps1boot.iso` + `.cue/.bin`: launch OK; kapotte cue geeft duidelijke preflight-fout.
- [ ] PS2 `.iso`/folder-dump: launch OK; folder-dump zonder marker volgt gedefinieerde pre-processing.
- [ ] PS3 JB/folder of ISO: correcte entry/boot; geen regressie in workaround.
- [ ] PSP `.iso/.cso/.pbp`: launch OK.
- [ ] Wii/Xbox/Amiga ISO: launch OK via iso-dispatcher.
- [ ] Niet-ISO (SNES/NES/GBA etc.): nog werkend via fallback/aparte route.
- [ ] Ontbrekende emulator/exec: duidelijke foutmelding, geen crash.
- [ ] Onveilige/ongeldige marker: geblokkeerd met duidelijke fout.

### E3. Acceptatiecriteria “klaar”
- [ ] ISO/disc games starten primair via iso-dispatcher.
- [ ] `game-gui-fork.py` heeft geen dubbele ISO-dispatch logica meer.
- [ ] Ongeldige markers/cue-referenties worden vóór emulatorstart afgevangen.
- [ ] `psx`/`ps1` mapping is transparant.
- [ ] Niet-ISO systemen zijn niet onbedoeld kapot.
