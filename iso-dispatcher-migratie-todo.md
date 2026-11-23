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
- [ ] Volgende: B2 entry-selectieregels verfijnen + B3 idempotent schrijven/rapportage; daarna B4 batch, Fase C `emu-launch.sh` adapter.

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
- [ ] Wijziging: implementeer deterministische entry-selectie.
- [ ] Regels: PS1/PS2 voorkeur `.cue` dan `.chd/.iso/.bin` (voorzichtig); PS3 JB `EBOOT.BIN`/`PS3_GAME/USRDIR/EBOOT.BIN`; PSP `.iso/.cso/.pbp` of `EBOOT.PBP`; generiek alleen bestaande relatieve paden.
- [ ] Test 1: Ace Combat 2 map → kiest `.cue`.
- [ ] Test 2: PS3 JB map → kiest EBOOT.
- [ ] Test 3: map met meerdere gelijkwaardige kandidaten → geen schrijven, ambigu-fout/lijst.

### B3. Idempotent schrijven + rapportage
- [ ] Wijziging: `--dry-run`, `--force`, leesbare rapportage/JSON. Bestaande correcte marker niet overschrijven.
- [ ] Test 1: run twee keer op zelfde map — verwacht: “created”, daarna “unchanged/skipped”.
- [ ] Test 2: bestaande andere marker zonder `--force` — verwacht: skip + waarschuwing.
- [ ] Test 3: met `--force` — verwacht: bewust overschreven, duidelijk gelogd.

### B4. Batch-modus voor bibliotheek
- [ ] Wijziging: marker-maker kan directory-recursief verwerken met `--limit`, `--dry-run`, rapport.
- [ ] Test: draai op kleine testsubset PSX-mappen — verwacht: rapport created/skipped/ambiguous/errors; geen crash; exit code weerspiegelt fouten.

## Fase C — `emu-launch.sh` herschrijven naar iso-dispatcher-adapter

**Bestand:** `/mnt/VG_00/PUBLIC-LIBRARY/PRE-INSTALLED-GAMES/WINEPREFIX/template-scripts/game-launchers/EMU-LAUNCHERS/emu-launch.sh`

### C1. Contract behouden, adapterlaag toevoegen
- [ ] Wijziging: behoud env-contract `GAME_SYSTEM`, `GAME_ROM`, `GAME_DISPLAY_NAME`, `GAME_EMU_CONF`. Voeg `ISO_DISPATCHER_BIN` override toe.
- [ ] Test: bestaand launch-commando vanuit shell met geldige env-vars werkt nog — verwacht: geen “missing env” regressie.

### C2. Systeem-mapping `fork -> iso-dispatcher class`
- [ ] Wijziging: maak expliciete mapping, minimaal `psx->ps1`; pass-through voor gelijke namen.
- [ ] Test 1: `GAME_SYSTEM=psx` + geldige marker/iso — verwacht: iso-dispatcher krijgt `--class ps1`.
- [ ] Test 2: `GAME_SYSTEM=ps2` — verwacht: `--class ps2`.

### C3. Folder-dump pre-processing behouden (tijdelijk)
- [ ] Wijziging: vóór iso-dispatcher: als `GAME_ROM` map is, hergebruik huidige logica (rom-in-map zoeken of `xdvdfs` cache-ISO maken); geef resulterend bestand aan iso-dispatcher.
- [ ] Test 1: map met `.cue/.iso` aanwezig → geen xdvdfs, juiste file gekozen.
- [ ] Test 2: echte folder dump zonder image maar met `xdvdfs` aanwezig → cache ISO gemaakt/hergebruikt.
- [ ] Test 3: map met `.psXboot.iso` marker → marker direct gebruiken, geen onnodige pack.

### C4. iso-dispatcher-first launch met fallback
- [ ] Wijziging: voor ISO/disc-geschikte cases: `iso-dispatcher launch <resolved> --class <mapped>`. Bij niet-ondersteund/ontbrekende iso-dispatcher: fallback naar oude emulation.conf-pad met duidelijke logregel.
- [ ] Test 1: PSX marker met werkende iso-dispatcher → launch via iso-dispatcher.
- [ ] Test 2: niet-ISO systeem (bv. SNES rom) → fallback of expliciete niet-ISO route, geen verkeerde iso-dispatcher poging.
- [ ] Test 3: iso-dispatcher binary tijdelijk niet beschikbaar → duidelijke fout/fallback, geen stille crash.

### C5. Preconf/install gedrag herplaatsen of behouden
- [ ] Wijziging: ISO-systemen via iso-dispatcher; niet-ISO/fallback behoudt `emu-install.sh` + `_apply_preconf`.
- [ ] Test 1: ontbrekende emulator in fallback-pad → `emu-install.sh` poging zoals vroeger.
- [ ] Test 2: ISO-systeem via iso-dispatcher, emulator aanwezig → geen onnodige install/preconf side effects.
- [ ] Test 3: bestaande userconfig blijft onaangetast (`cp -rn` gedrag behouden waar van toepassing).

### C6. Logging/exit codes normaliseren
- [ ] Wijziging: geef iso-dispatcher stdout/stderr door; behoud herkenbare `[emu]`/`[game]` prefixen waar fork/UI op leunt.
- [ ] Test: start vanuit game-gui-fork, bekijk terminaltab — verwacht: duidelijke status, leesbare fouten, exitcode correct.

## Fase D — `game-gui-fork.py` en gerelateerde files aanpassen

**Bestanden:** `game-gui-fork.py`, `EMU-LAUNCHERS/emulation.conf`, `~/.config/gamelauncher/emulators.json`, `README-game-gui-fork.md`, `gui-version/changelog.md`

### D1. `_start_emu` geschikt maken voor iso-dispatcher flow
- [ ] Wijziging: voor ISO/disc systems direct `iso-dispatcher launch` of via aangepaste `emu-launch.sh`; houd één pad leidend.
- [ ] Wijziging: map `psx` UI-systeem naar iso-dispatcher `ps1` bij directe call.
- [ ] Test 1: klik PSX game/marker in fork → emulator start.
- [ ] Test 2: PS2/PSP/Wii/Xbox smoke test via fork.
- [ ] Test 3: draaiende game dubbel starten → bestaande “draait al/focus” gedrag blijft werken.

### D2. Ontbrekende-emulator afhandeling aanpassen
- [ ] Wijziging: als iso-dispatcher exec ontbreekt, geef fout met class/exec uit iso-dispatcher config. Houd `emu-install.sh` alleen waar fallback/conf-pad dat nog gebruikt.
- [ ] Test: zet tijdelijk een ongeldige exec in iso-dispatcher config voor testclass — verwacht: duidelijke GUI-melding, geen stille failure.

### D3. Config-afhankelijkheden reduceren
- [ ] Wijziging: `load_emulation_conf`/`emulation_conf_path` niet meer verplicht voor ISO-systems. Gebruik iso-dispatcher config/CLI waar nodig.
- [ ] Wijziging: behoud conf alleen voor niet-ISO systemen tijdens migratie.
- [ ] Test 1: ISO-systeem start zonder geldige `emulation.conf` entry voor dat systeem — verwacht: werkt via iso-dispatcher.
- [ ] Test 2: niet-ISO systeem met conf/fallback — verwacht: werkt nog zoals voorheen.

### D4. Marker-first game-detectie
- [ ] Wijziging: scanner/UI prefereert `.psXboot.iso` markers als startbare items; voorkom dubbel tonen van marker + onderliggende rom waar ongewenst.
- [ ] Test 1: map met marker + cue/bin toont één startbaar item (of duidelijke regel).
- [ ] Test 2: marker launch gebruikt entry uit marker correct.

### D5. Metadata/titels/icons zonder emulation.conf
- [ ] Wijziging: systeemtitels/icons uit fork-mapping/iso-dispatcher of eigen lichte mapping halen; niet afhankelijk van volledige emulation.conf voor ISO systems.
- [ ] Test: nav toont correcte platform/systeemtitels en icons bij afwezigheid van ISO-conf entries.

### D6. Docs/changelog bijwerken
- [ ] Wijziging: update `README-game-gui-fork.md`, relevante workflowdocs, `gui-version/changelog.md`.
- [ ] Test: leescheck — docs beschrijven nieuwe flow (iso-dispatcher-first, marker-maker, fallback, psx/ps1 mapping).

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
