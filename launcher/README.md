# Snake Launcher (customtkinter + ctypes)

This folder contains the desktop launcher for Snake. It supports:

- Human mode with custom grid size and optional seed
- Bot mode with cycle generation, TPS control, and tuning presets

The launcher depends on the `snakebot` shared library for cycle generation.

## Build output layout

The build scripts produce:

```
build/
  launcher(.exe)
  <standalone GUI deps>
  libsnakebot.so | snakebot.dll
  game/
    snake(.exe)
```

Run `build/launcher(.exe)` to start the launcher. The launcher will look for the
`snake` executable under `build/game/`.

## Run the launcher (dev mode)

```bash
python -m venv .venv
source .venv/bin/activate   # Windows: .venv\\Scripts\\activate
pip install -r requirements.txt
python main.py
```

When running in dev mode, make sure `libsnakebot.so` / `snakebot.dll` is in the
same directory as `main.py` (or next to the built executable if packaged).

## Nuitka standalone packaging (example)

Make sure you have `python3-dev`, `patchelf`, and `python3-tk` installed on Linux.

From inside `launcher/`:

```bash
python -m nuitka \
  --standalone \
  --enable-plugin=tk-inter \
  --include-package=customtkinter \
  --output-dir=dist \
  main.py
```

Then copy `snakebot.dll` / `libsnakebot.so` next to the produced executable.

## .cycle file format

The `.cycle` file is plain ASCII text with a small header + metadata + the
direction letters.

Example:

```
SNAKECYCLE 1
width=20
height=20
window_w=800
window_h=600
seed=12345
cycle_type=maze
wrap=0
DATA
...
```

The DATA section is **one direction letter per cell** in row-major order
(`y*w + x`):

- Letters: `U D L R`
- Exactly `w*h` letters (whitespace ignored by the validator)

The launcher and game refuse to load any file that does not end in `.cycle`.
The game reads this file via `--bot-cycle cycle.cycle`.
