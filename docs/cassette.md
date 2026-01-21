# Cassette Model

This emulator models the Altaid cassette interface as a **digital level stream**, not as audio.

- The ROM drives cassette output by writing bit 0 to `OUT 0x44`.
- Cassette input is sampled as a digital level on bit 6 of `IN 0x40`.

The `src/cassette.c` implementation records and replays the **timing of level changes** (edge-to-edge durations) in CPU ticks.

## File format: ALTAP001

Cassette files use a small custom container (magic `ALTAP001`). The intent is:

- deterministic replay
- easy debugging (edge times are stored explicitly)
- independence from audio sample rates / filtering

## CLI

Attach a cassette file and optionally start recording or playback at tick 0:

```sh
./altaid-emu my64k.rom --cass demo.ALTAP001 --cass-rec  --pty --panel
./altaid-emu my64k.rom --cass demo.ALTAP001 --cass-play --pty --panel
```

If you need WAV/audio conversion, keep that as a separate “tooling” layer that converts between audio and this digital edge stream.

## Ctrl-P commands (runtime)

All cassette transport operations are available at runtime via the Ctrl-P prefix
(both in stdio mode and in the full-screen UI):

- `Ctrl-P a` : set/attach cassette filename (interactive prompt)
- `Ctrl-P P` : play
- `Ctrl-P R` : record
- `Ctrl-P K` : stop
- `Ctrl-P W` : rewind
- `Ctrl-P J` : fast-forward 10s
- `Ctrl-P V` : save tape image now (flush ALTAP001)

Notes:

- Transport commands require an attached cassette file.
- Recording changes are automatically written on exit if `--cass-rec` is used.
