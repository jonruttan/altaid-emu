# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog and this project adheres to Semantic Versioning.

## [Unreleased]

### Added
- Project documentation (`docs/`): port map, memory map, cassette notes, fidelity notes, troubleshooting.
- OSS hygiene: `LICENSE`, `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `SECURITY.md`, issue templates.
- CI build workflow (macOS + Linux).
- Real-time throttling (default) and `--turbo` mode.

### Fixed
- Reduced host CPU usage by batching CPU execution and avoiding per-instruction host polling.
- In `--realtime` mode, throttling sleep now wakes early on host input (stdin/PTY) via `select()`.

### Changed
- UI/panel output moved to **stderr** to keep stdout usable for piping serial output.
- 8080 EI pending state is now per-CPU instance (no global).


## [0.1.1] - 2026-01-17

### Changed
- Renamed `-ansi` to `-ui` (kept `-ansi` as a temporary alias with a warning).
- Added `Ctrl-P u` front-panel command to toggle UI mode at runtime.
- Panel renderer can switch between text and UI mode without restarting.

## [0.1.0] - 2026-01-17

- Initial public snapshot.
