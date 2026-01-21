# Troubleshooting

## Windows note

This project targets **Linux** and **macOS**. Windows support is best-effort only; the project does not accept Windows-only workarounds as a requirement for v1.0.

If you want to run on a Windows host, prefer a Unix-like environment (for example, a POSIX compatibility layer) so the build and terminal behavior match the emulator’s assumptions.


## Build fails

- This is a small C99 project. Try `make clean && make`.
- On macOS, ensure Command Line Tools are installed.

## I see no output / no boot banner

- Ensure your ROM image is **exactly 65536 bytes**.
- Try running without the panel first:

  ```sh
  ./altaid-emu my64k.rom
  ```

- If you are using `--ui`, note that the emulator writes UI output to **stderr**.

## Serial looks garbled

- Confirm the ROM and the emulator use the same baud rate (`--baud`, default 9600).
- In `--pty` mode, connect with:

  ```sh
  screen /dev/pts/XXX 9600
  ```

## PTY mode prints a path but my terminal program gets nothing

- Some systems need a slave side kept open. The emulator opens the slave in raw mode to avoid EIO.
- If you still see nothing, try another terminal program (e.g. `screen` vs `minicom`).

## Cassette load fails

- Confirm you attached a file with `--cass` and selected `--cass-play` or `--cass-rec`.
- This emulator uses a **digital edge-timing file**, not WAV audio.
