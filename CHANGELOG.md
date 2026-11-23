# Changelog

All notable changes to `iso-dispatcher` are documented in this file.

## Versioning & builds

- **Version**: `MAJOR.MINOR.PATCH` (current: `0.0.6`)
- **Versioned builds**: place built binaries in `build/Build-versions/`
- **Naming**: `iso-dispatcher-<version>-linux-x86_64` (append `-<shortsha>` or `-<date>` if needed for same-version dev snapshots)
- **On new change/build**: bump version in source (`src/cli/Cli.cpp` about/version, `src/ui/MainWindow.cpp` About), update this changelog, rebuild, and copy the new binary to `build/Build-versions/` with the new version name. Keep previous versioned binaries for traceability.

## [0.0.6] - 2026-11-23

### Fixed
- `IsoProbe`: accept directory input when it contains a boot marker (`*.ps1/ps2/ps3/psp/psxboot.iso`), resolving to that marker instead of failing with "file not found".

### Verified
- PS3 JB dir `BLES-01236-SONIC_GENERATIONS(PS3)` with `ps3boot.iso` marker: `launch --class ps3 --dry-run` resolves marker/entry/EBOOT correctly.

## [0.0.5] - 2026-11-23

### Added
- `make-marker --batch <dir>`: recursive batch marker generation with `--limit`, `--dry-run`, `--force`, `--json`.
- Batch JSON/text report with counts: `created/overwritten/unchanged/skipped/ambiguous/errors/proposed`.

### Changed
- Batch mode skips irrelevant directories (no game candidates) and returns exit code 1 when ambiguous/error items are present.

### Verified
- Temp PSX subset: dry-run reported 3 processed, 2 proposed, 1 ambiguous; real run created 2 + ambiguous 1; rerun unchanged 2 + ambiguous 1; `--limit 1` processed 1; no crash.

## [0.0.4] - 2026-11-23

### Added
- `make-marker` B3: `--force` flag, idempotent write/report behavior.
- JSON output now includes `action`, `dryRun`, `force`.

### Changed
- `make-marker` reports clear actions: `Created`, `Skipped: marker already up-to-date`, warning+skip for existing different marker without `--force`, `Overwritten` with `--force`.
- Dry-run with `--force` on existing different marker reports `would overwrite`.

### Verified
- Same dir twice → `Created`, then `unchanged`/`Skipped`.
- Existing different marker without `--force` → warning, exit 0, content unchanged.
- With `--force` → `Overwritten`, content replaced; `--force --dry-run --json` reports `action=overwrite`.

## [0.0.3] - 2026-11-23

### Added
- **Marker maker (B1/B2 baseline)**: `make-marker` CLI subcommand (iso-dispatcher) for boot-marker proposal/creation, with `--class/--system`, `--dry-run`, `--json`.
- Deterministic entry selection: PS1/PS2 prefer `.cue`, then `.chd/.iso/.bin`; PS3 JB prefers `PS3_GAME/USRDIR/EBOOT.BIN`/`EBOOT.BIN`; PSP prefers `.iso/.cso/.pbp` or `EBOOT.PBP`; ambiguous candidates return error/list (no write).
- README (EN/NL): marker-maker documentation added.

### Verified (B2)
- PS1/PS2: Ace Combat 2 map + `--class ps1 --dry-run` → kiest `.cue` (`proposed`, no write).
- PS3 JB: `BLES00876-Toy Story 3` + `--class ps3 --dry-run` → kiest `PS3_GAME/USRDIR/EBOOT.BIN` (`unchanged`, existing marker up-to-date); fresh temp PS3 JB dir → `proposed` with same EBOOT entry.
- Ambiguity: temp dir with multiple equal `.cue` candidates → `ambiguous`, candidate list, no write.

## [0.0.2] - 2026-11-23

### Added
- `make-marker` CLI subcommand (aliases: `makemarker`, `make_marker`) for boot-marker proposal/creation (Fase B1).
- `MarkerMaker` core class (`src/core/MarkerMaker.{h,cpp}`) with deterministic candidate selection foundation for B2-B4.
- `make-marker` flags: `--class/--system`, `--dry-run`, `--json`; statuses: `proposed`, `unchanged`, `exists`, `ambiguous`, `error`.
- Initial entry-selection heuristics: PS1/PS2 prefer `.cue` then `.chd/.iso/.bin`; PS3 prefers `PS3_GAME/USRDIR/EBOOT.BIN`/`EBOOT.BIN`; PSP prefers `.iso/.cso/.pbp`/`EBOOT.PBP`; existing `*boot.iso` files excluded as candidates.

### Verified
- `make-marker "/.../PSX/Ace_Combat_2" --class ps1 --dry-run` → proposed `Ace_Combat_2.ps1boot.iso`, entry `Ace_Combat_2_[SCES-00699].cue`, no write.
- `make-marker "/.../PSX/Ace_Combat_2" --dry-run` without class → clear error asking for `--class`.

## [0.0.1] - 2026-11-23

### Added
- Boot-marker preflight: header `ISO-DISPATCHER-BOOT`, required `class`/`entry`, clear validation errors.
- Safe entry resolution: relative to marker dir, blocks absolute paths and `..` traversal; checks existence/readability.
- Cue reference validation: validates `FILE` entries referenced by `.cue` before launch.
- Class alias normalization: `psx -> ps1`.
- Deterministic CLI exit codes: `0` ok, `1` general/usage, `2` invalid marker, `3` missing/invalid entry/reference, `4` unknown/unconfigured class, `5` missing exec/launch failed.
- `launch --dry-run` preflight/resolution output (chosen class, boot marker, resolved entry, exec, args) without starting emulator.
- GUI language menu with English default (`en`) and Dutch (`nl`); config `language` field.
- Class `priority` is now an active ranking tiebreaker (`total` desc, `base` desc, `priority` desc); shown in CLI explain and GUI scores table.
- Versioned build directory: `build/Build-versions/`.

### Changed
- CLI/core user-facing messages normalized to English.
- CLI about/version output includes `iso-dispatcher 0.0.1`.
- GUI About dialog includes version `0.0.1`.

### Verified
- Build OK via `./build.sh`.
- Boot-marker regressions: 19/19 sampled existing markers (7 PS3, 12 PS1) classify OK with correct class/bootEntry.
- Preflight negative tests: missing entry (2), traversal (2), missing cue FILE (3), unknown class (4), missing exec (5).
