# Changelog

All notable changes to `iso-dispatcher` are documented in this file.

## Versioning & builds

- **Version**: `MAJOR.MINOR.PATCH` (current: `0.0.1`)
- **Versioned builds**: place built binaries in `build/Build-versions/`
- **Naming**: `iso-dispatcher-<version>-linux-x86_64` (append `-<shortsha>` or `-<date>` if needed for same-version dev snapshots)
- **On new change/build**: bump version in source (`src/cli/Cli.cpp` about/version, `src/ui/MainWindow.cpp` About), update this changelog, rebuild, and copy the new binary to `build/Build-versions/` with the new version name. Keep previous versioned binaries for traceability.

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
