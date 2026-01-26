# Altaid 8800 Emulator

Terminal-based emulator for the **Altaid 8800** (Intel 8080): cycle-timed CPU, ALTAID05-style memory/port map, bit-level serial, cassette, and a multiplexed front panel UI.

- Hardware reference: Sunrise EV Altaid 8800 (projects page)
- Docs:
  - Specs: `docs/spec/project_spec_current.md` (authoritative),
    `docs/spec/project_spec_v1.md` (roadmap), `docs/spec/workflow.md` (workflow)
  - Hardware / I/O: `docs/port-map.md`, `docs/memory-map.md`, `docs/cassette.md`,
    `docs/persistence.md`, `docs/fidelity.md`
  - Troubleshooting: `docs/troubleshooting.md`
  - Contributing: `docs/spec/style.md`, `docs/architecture.md`,
    `docs/spec/release.md`

## Project philosophy

- **Keep it small:** Prefer the minimum number of tools/docs needed to ship a credible emulator.
- **Single source of truth:** `docs/spec/project_spec_current.md` defines current behavior. The v1 spec is aspirational only.
- **AI/reviewers:** When scanning the project, treat the current spec as authoritative; do not assume roadmap items exist unless they appear in the current spec.


## Requirements

This project aims to be **Unix-friendly** and keep dependencies minimal.

You need:
- A **C compiler** (`cc`, `gcc`, `clang`, etc.)
- **GNU make**
- Common shell tools used by build/test scripts: `sh`, `find`, `sort`, `stat`
- `git` (optional, but suggested)

### Notes by OS

- **macOS (easy path):** install Apple’s Command Line Tools (provides `cc`/`clang`, headers/SDK, and `/usr/bin/make`).
  - Install: `xcode-select --install`
  - Verify: `make --version` and `cc --version`

- **Linux:** install a compiler toolchain + GNU make (package names vary by distro). Examples:
  - Debian/Ubuntu: `sudo apt-get update && sudo apt-get install build-essential`
  - Fedora: `sudo dnf groupinstall "Development Tools"`
  - Arch: `sudo pacman -S base-devel`
  - If you prefer clang: install `clang` and set `CC=clang` (for example: `make CC=clang`).

- **Windows:** best experience is usually via **WSL2** (Ubuntu/Debian/etc.). Example (inside WSL):
  - `sudo apt-get update && sudo apt-get install build-essential`

If you already have a compiler + GNU make, you’re good — the emulator doesn’t depend on any IDE.

## Quick start

### 1) Build

```sh
make
```

### 1a) Tests (optional)

Tests use **test-runner** harness (git submodule at `tests/test-runner`).

```sh
git submodule update --init --recursive
make test-all
# Run a single spec file:
TESTS=tests/e2e/smoke.spec.c make test
# Instrumented run (valgrind/lldb/etc. via WRAPPER):
make test-wrapped
make test-wrapped-all
```

### Troubleshooting (common first-run issues)

- **`make: cc: command not found` / no compiler**
  - You need a C toolchain (`cc`/`clang`/`gcc`).
  - macOS (easy path): `xcode-select --install`
  - Debian/Ubuntu/WSL: `sudo apt-get update && sudo apt-get install build-essential`

- **Tests fail with: `ERROR: test runner not found ...`**
  - The test runner is a git submodule:

    ```sh
    git submodule update --init --recursive
    ```

- **Tests fail with: `ERROR: no tests matched ...`**
  - Check that test files exist:

    ```sh
    ls -la tests/unit/*.spec.c tests/e2e/*.spec.c
    ```

- **UI looks garbled / borders don’t line up**
  - Try forcing ASCII borders: `--ascii`
  - Make sure your terminal is at least **80×25** (or override with `-x/-y`).

- **No banner / nothing seems to happen**
  - Verify your ROM is **exactly 65536 bytes**:

    ```sh
    ls -l my64k.rom
    ```

  - For more detail, log diagnostics: `./altaid-emu my64k.rom --log emu.log`

For more, see `docs/troubleshooting.md`.


### 2) Get a 64 KiB ROM image

This repo **does not distribute ROM images**.

You must provide a **65536-byte** ROM image (e.g. an ALTAID05 family ROM) and pass it as the **first positional argument**.

### 3) Run

```sh
./altaid-emu my64k.rom
```

You should see a boot banner such as `COLD-BOOT` / `Altaid 8800 v...`.

## Usage

```text
./altaid-emu <rom64k.bin> [options]
```

Core options:
- `-C, --hz <cpu_hz>`: CPU clock in Hz (default: 2000000)
- `-b, --baud <baud>`: serial baud (default: 9600)

Panel options:
- `-p, --panel`: enable front-panel display (**UI output goes to stderr**)
- `-m, --panel-mode <burst|change>`: text-mode panel output policy (default: `burst`)
- `-E, --panel-echo-chars`: in `burst` mode, also snapshot on single-character echoes (noisy)
- `-u, --ui`: ANSI color + cursor control (split-screen when refreshing)
- `--no-altscreen`: in `--ui` refresh mode, do not use the alternate screen buffer
- `--ascii`: in `--ui` mode, force ASCII panel rendering (borders + LED glyphs) instead of Unicode
- `--panel-hz <n>`: refresh rate override (`0` snapshot, `N>0` live)
- `-y, --term-rows <n>` / `-x, --term-cols <n>`: override terminal size used for ANSI layout (takes precedence over probing)

Serial options:
- `-t, --pty`: expose emulated serial via a host PTY (for `screen`, `minicom`, `lrzsz`, etc.)
- `-I, --pty-input`: in `--pty` mode, allow local keyboard input as serial RX
- `-S, --serial-fd <stdout|stderr>`: choose terminal stream for decoded TX bytes (non-PTY only)
- `-o, --serial-out <dest>`: send decoded TX bytes to `stdout`, `stderr`, `-`, `none`, or a file path
  - In `--pty` mode, this is a **mirror** (PTY remains primary)
- `-a, --serial-append`: when `--serial-out` is a file, append instead of truncating

Cassette options:
- `-c, --cass <file>`: attach cassette file (ALTAP001)
- `-L, --cass-play`: start playing at tick 0
- `-R, --cass-rec`: start recording at tick 0 (overwrites on exit)

Persistence options:
- `-s, --state-file <file>`: default machine state file used by Ctrl-P commands
- `-J, --state-load <file>`: load machine state at startup
- `-W, --state-save <file>`: save machine state on exit
- `-M, --ram-file <file>`: default RAM file used by Ctrl-P commands
- `-G, --ram-load <file>`: load RAM banks at startup
- `-B, --ram-save <file>`: save RAM banks on exit

Other:
- `--hold <ms>`: momentary front-panel key press duration (default: 50)
- `--realtime`: throttle emulation to real-time based on `--hz` (**default on**)
- `--turbo`: run as fast as possible (may use 100% CPU)
  - In `--realtime` mode the emulator sleeps using `select()` and wakes early on keyboard/PTY input, so it stays responsive without burning CPU.
- `-l, --log <file>`: write diagnostics to a log file
- `-q, --quiet`: suppress most diagnostics
- `-n, --headless`: do not enter terminal raw mode and do not enable front-panel keybindings

Common short options: `-h` (help), `-V` (version), `-p` (panel), `-u` (ui), `-t` (pty), `-o` (serial out).

## Golden paths

### A) Minimal boot

```sh
./altaid-emu my64k.rom
```

### B) Full experience: ANSI panel + PTY

```sh
./altaid-emu my64k.rom --pty --panel --ui
# In another terminal:
screen /dev/pts/XXX 9600
```

### C) Cassette record/play

```sh
./altaid-emu my64k.rom --pty --panel --cass demo.ALTAP001 --cass-rec
# (save something from the ROM monitor)

./altaid-emu my64k.rom --pty --panel --cass demo.ALTAP001 --cass-play
```

## Streams: stdout vs stderr (important)

By default, the emulator tries to keep things usable for both humans and pipes:

- **Decoded serial TX bytes** default to **stdout**.
- **Panel/UI output + diagnostics** default to **stderr** (or `--log <file>`).
- When an ANSI UI panel is actively rendered (`--panel --ui`), the emulator keeps the UI and any
  terminal-visible serial bytes on the **same stream** to prevent escape-sequence interleaving.

If you want explicit control:

- `--serial-out <dest>` sends decoded serial TX bytes to a chosen destination (`stdout`, `stderr`, `-`, `none`, or a file).
- `--serial-fd <stdout|stderr>` selects the terminal stream for decoded serial TX bytes (non-PTY only).

If your goal is a clean serial capture, prefer `--serial-out <file>` (optionally `--serial-append`) or `--headless`.

## Front-panel controls (keyboard)

- Stdio mode (default): use **Ctrl-P** as a prefix, then press:
  - `1`..`8`: DATA keys D0..D7
  - `r`: RUN
  - `m`: MODE
  - `n`: NEXT
  - `N`: NEXT + D7 chord ("back" in some monitors)
  - `p`: toggle panel display
  - `s` / `l`: save/load machine state (`--state-file`)
  - `f`: set state filename (interactive prompt)
  - `b` / `g`: save/load RAM banks (`--ram-file`)
  - `M`: set RAM filename (interactive prompt)
  - `a`: set/attach cassette filename (interactive prompt)
  - `P` / `R` / `K`: cassette Play / Record / stop
  - `W` / `J`: cassette Rewind / fast-forward 10s
  - `V`: save tape image now
  - `u`: toggle UI mode (`--ui`) at runtime
  - `Ctrl-R`: reset emulated machine
  - `d`: dump a one-shot snapshot
  - `?` / `h`: help
  - `q`: quit

- PTY mode (`--pty`): defaults to a **read-only control deck** (panel keys are direct). Press **Ctrl-P t** to toggle local serial typing. (`p` toggles the panel; `u` toggles UI mode.)

See also:
- `docs/persistence.md`
- `docs/cassette.md`

## Hardware model (summary)

- 8 × 64 KiB RAM banks (512 KiB)
- 64 KiB ROM image split into two 32 KiB halves
- ROM overlays (shadow RAM): writes always go to RAM; reads can come from ROM

See:
- `docs/memory-map.md`
- `docs/port-map.md`
- `docs/fidelity.md`

## License

MIT (see `LICENSE`).

See `CREDITS.md` for project credits.

## Coding style

See `docs/spec/style.md` for the coding style used in this repo.

## Contributing

See `CONTRIBUTING.md`.

## Architecture

See `docs/architecture.md` for an overview of the core/host/UI split and test seams.
