# snake-sdl

A cross-platform Snake game written in C using SDL3, featuring smooth interpolation,
wrap-around movement, and clean grid snapping.

Prebuilt binaries are provided for Linux and Windows on the Releases page.

---

## Downloads

Prebuilt binaries are available on the Releases page:

- Windows: snake-windows.exe
- Linux: snake-linux

https://github.com/ManifestJW/snake-sdl/releases

---

## Running the prebuilt binaries

### Linux (x86_64)

1. Download snake-linux
2. Make it executable:
   chmod +x snake-linux
3. Run it from a graphical session:
   ./snake-linux

#### Linux runtime dependencies

The Linux binary is dynamically linked and depends on common system libraries
(X11 / Wayland, OpenGL/EGL, and ALSA).

On a clean Ubuntu / Pop!_OS system, install:

sudo apt-get update
sudo apt-get install -y \
  libx11-6 libxext6 libxrandr2 libxrender1 libxss1 \
  libxcursor1 libxi6 libxinerama1 libxkbcommon0 \
  libsm6 libice6 \
  libwayland-client0 libwayland-cursor0 libwayland-egl1 \
  libegl1 libgl1 \
  libasound2

You must run the game from a graphical session (Wayland or X11).
Running from a TTY or over SSH without display forwarding will fail.

To inspect runtime dependencies on your system:

ldd ./snake-linux

---

### Windows (x86_64)

1. Download snake-windows.exe
2. Double-click to run (or launch from Command Prompt)

#### Windows runtime dependencies

None.

The Windows build uses a static MSVC runtime (/MT) and runs on a clean
Windows installation without additional DLLs.

---

## Building from source

### Requirements (all platforms)

- C compiler
- CMake 3.21 or newer
- Ninja
- vcpkg (manifest mode)

---

### Windows build (MSVC)

#### Prerequisites

- Windows 10 or 11
- Visual Studio 2022 with "Desktop development with C++"
- Git

#### Build steps

git clone https://github.com/<OWNER>/<REPO>.git
cd snake-sdl

cmake -S . -B build ^
  -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=%CD%\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static

cmake --build build

Resulting executable:

build/snake.exe

---

### Linux build (Ubuntu / Pop!_OS)

#### Build dependencies

sudo apt-get update
sudo apt-get install -y \
  autoconf autoconf-archive automake libtool \
  pkg-config python3-venv libltdl-dev \
  # X11 + input
  libx11-dev libxext-dev libxrandr-dev libxrender-dev libxss-dev \
  libxcursor-dev libxi-dev libxinerama-dev libxkbcommon-dev \
  libsm-dev libice-dev libxtst-dev \
  # Wayland
  libwayland-dev wayland-protocols libdecor-0-dev \
  # OpenGL / EGL
  libegl1-mesa-dev libgl1-mesa-dev \
  # Audio
  libasound2-dev

#### Build steps

git clone https://github.com/<OWNER>/<REPO>.git
cd snake-sdl

cmake -S . -B build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux

cmake --build build

Resulting executable:

build/snake

---

## Notes on portability

- Linux binaries are dynamically linked to system graphics and audio libraries.
  This keeps binaries small and compatible with most distributions, but requires
  the listed runtime packages.
- Windows binaries are fully self-contained.
- SDL video backends (X11 / Wayland) are enabled at build time based on available
  system headers.

---

## License

MIT License. See LICENSE for details.

This project uses SDL3, which is licensed under the zlib license.
