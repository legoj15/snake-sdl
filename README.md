# snake-sdl

A cross-platform Snake game written in C using SDL3, featuring smooth interpolation,
wrap-around movement, and clean grid snapping.

It also includes a bot with a dedicated launcher for cycle generation, tuning presets,
and human-mode customization.

Prebuilt binaries are provided for Linux and Windows on the Releases page.

---

## Downloads

Prebuilt binaries are available on the Releases page:

- Windows: snake-windows.zip
- Linux: snake-linux.zip

https://github.com/ManifestJW/snake-sdl/releases

---

## Running the prebuilt binaries

It is recommended to launch the game through the launcher so you can configure
human mode and bot mode settings. See `launcher/README.md` for details.

### Linux (x86_64)

1. Download `snake-linux.zip`
2. Unzip it
3. Make the launcher executable:
   chmod +x launcher
4. Run it from a graphical session:
   ./launcher

This build is for x86_64 Linux only. macOS is not supported.

#### Linux runtime dependencies

The Linux binary is dynamically linked and depends on common system libraries
(X11 / Wayland, OpenGL/EGL, and ALSA).

On a Ubuntu / Pop!_OS system, install:

sudo apt-get update
sudo apt-get install -y \
  libx11-6 libxext6 libxrandr2 libxrender1 libxss1 \
  libxcursor1 libxi6 libxinerama1 libxkbcommon0 \
  libsm6 libice6 \
  libwayland-client0 libwayland-cursor0 libwayland-egl1 \
  libegl1 libgl1 \
  libasound2

On Fedora/RHEL:

sudo dnf install -y \
  libX11 libXext libXrandr libXrender libXScrnSaver \
  libXcursor libXi libXinerama libxkbcommon \
  libSM libICE \
  wayland wayland-client wayland-cursor \
  mesa-libEGL mesa-libGL \
  alsa-lib

On Arch:

sudo pacman -S --needed \
  libx11 libxext libxrandr libxrender libxscrnsaver \
  libxcursor libxi libxinerama libxkbcommon \
  libsm libice \
  wayland \
  mesa \
  alsa-lib

You must run the game from a graphical session (Wayland or X11).
Running from a TTY or over SSH without display forwarding will fail.

To inspect runtime dependencies on your system:

ldd ./snake-linux

---

### Windows (x86_64)

1. Download `snake-windows.zip`
2. Unzip it
3. Double-click `launcher.exe` to run (or launch from Command Prompt)

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
- Python 3.13+ (for building the launcher)

---

### Windows build (MSVC)

#### Prerequisites

- Windows 10 or 11
- Visual Studio 2022 with "Desktop development with C++"
- Git

#### Build steps

git clone https://github.com/<OWNER>/<REPO>.git
cd snake-sdl
build-windows.bat release

---

### Linux build (Ubuntu / Pop!_OS)

#### Build dependencies

sudo apt-get update
sudo apt-get install -y \
  autoconf autoconf-archive automake libtool \
  pkg-config zip \
  python3-venv python3-dev python3-tk patchelf libltdl-dev \
  libx11-dev libxext-dev libxrandr-dev libxrender-dev libxss-dev \
  libxcursor-dev libxi-dev libxinerama-dev libxkbcommon-dev \
  libsm-dev libice-dev libxtst-dev \
  libwayland-dev wayland-protocols libdecor-0-dev \
  libegl1-mesa-dev libgl1-mesa-dev \
  libasound2-dev

On Fedora/RHEL:

sudo dnf install -y \
  autoconf autoconf-archive automake libtool \
  pkgconf zip \
  python3-virtualenv python3-devel python3-tkinter patchelf libtool-ltdl-devel \
  libX11-devel libXext-devel libXrandr-devel libXrender-devel libXScrnSaver-devel \
  libXcursor-devel libXi-devel libXinerama-devel libxkbcommon-devel \
  libSM-devel libICE-devel libXtst-devel \
  wayland-devel wayland-protocols-devel libdecor-devel \
  mesa-libEGL-devel mesa-libGL-devel \
  alsa-lib-devel

On Arch:

sudo pacman -S --needed \
  autoconf autoconf-archive automake libtool \
  pkgconf zip python python-virtualenv tk patchelf libltdl \
  libx11 libxext libxrandr libxrender libxscrnsaver \
  libxcursor libxi libxinerama libxkbcommon \
  libsm libice libxtst \
  wayland wayland-protocols libdecor \
  mesa \
  alsa-lib

#### Build steps

git clone https://github.com/<OWNER>/<REPO>.git
cd snake-sdl
./build.sh release

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
