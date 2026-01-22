# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project aims to follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- 

### Changed
- 

### Fixed
- 


## [2.1.0] - 2026-01-21

### Added
- Background music playback with asset packaging.
- Launcher toggle to enable/disable BGM for human and bot modes.
- Optional audio codec support (opus, vorbis, mpg123, fluidsynth, xmp).

### Changed
- BGM assets now load from `game/assets` in the packaged build layout.
- Build scripts copy `assets/` into the packaged game directory.

## [2.1.0-hotfix.1] - 2026-01-21

### Fixed
- Exit-time memory corruption tied to base path cleanup.

## [2.1.1] - 2026-01-21

### Changed
- Release packaging now zips downloaded artifacts during the release job to prevent missing uploads.

### Fixed
- Win screen now always fills the grid without intermittent blank cells.

## [2.1.1-hotfix.1-test.1] - 2026-01-21

### Changed
- Log output now goes to `game/logs/snake.log`.


## [2.0.0-hotfix.2] - 2026-01-21

### Fixed
- Launcher path finding now targets the packaged game layout.


## [2.0.0-hotfix.1] - 2026-01-21

### Fixed
- Dependency packaging issues that caused runtime errors.


## [2.0.0] - 2026-01-21

### Added
- A Bot that always wins
- Bot launcher with presets and tuning sliders with reset controls.
- Multiple Hamiltonian cycle types (serpentine, spiral, maze, scrambled) with wrap fallback validation.
- Human-mode launcher tab with custom grid size and optional seed.
- Auto window sizing with 1080p cap and grid cell scaling.
- Discrete bot TPS steps with live value display.

### Changed
- Build output layout now uses `build/launcher(.exe)` and `build/game/snake(.exe)`.
- Build scripts now install vcpkg deps, build the game + launcher, and package the launcher with dependencies.
- Launcher folder renamed from `bot_gui` to `launcher`.
- CI builds now run the scripts, verify layout, package zips, and attach release notes from the changelog.

### Fixed
- Grid toggle stays available during win/death states.
- Win screen fills the last cell correctly when the snake reaches full length.
- Bot behavior handles length 1 without forced cycle fallback.


## [1.0.0] - 2026-01-18

### Added
- Initial public release.
- Cross-platform builds (Linux + Windows) via GitHub Actions.
- Snake rendering with interpolation and wrap-aware bridges.
- Apple spawning and body growth.
- Optional snapped head rendering style.

[Unreleased]: https://github.com/ManifestJW/snake-sdl/compare/v2.1.1-hotfix.1-test.1...HEAD
[2.1.1-hotfix.1-test.1]: https://github.com/ManifestJW/snake-sdl/releases/tag/v2.1.1-hotfix.1-test.1
[2.1.1]: https://github.com/ManifestJW/snake-sdl/releases/tag/v2.1.1
[2.1.0-hotfix.1]: https://github.com/ManifestJW/snake-sdl/releases/tag/v2.1.0-hotfix.1
[2.1.0]: https://github.com/ManifestJW/snake-sdl/releases/tag/v2.1.0
[2.0.0-hotfix.2]: https://github.com/ManifestJW/snake-sdl/releases/tag/v2.0.0-hotfix.2
[2.0.0-hotfix.1]: https://github.com/ManifestJW/snake-sdl/releases/tag/v2.0.0-hotfix.1
[2.0.0]: https://github.com/ManifestJW/snake-sdl/releases/tag/v2.0.0
[1.0.0]: https://github.com/ManifestJW/snake-sdl/releases/tag/v1.0.0
