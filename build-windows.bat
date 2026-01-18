@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM ============================
REM Defaults (kept for symmetry / future use)
REM ============================
if not defined BUILD_DIR set BUILD_DIR=build
if not defined BUILD_TYPE set BUILD_TYPE=Debug

REM ============================
REM Resolve vcpkg toolchain
REM ============================
if defined VCPKG_TOOLCHAIN_FILE (
    set "TOOLCHAIN=%VCPKG_TOOLCHAIN_FILE%"
) else if defined VCPKG_ROOT (
    set "TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
) else (
    echo ERROR: vcpkg not configured.
    echo.
    echo Set one of:
    echo   VCPKG_ROOT=path\to\vcpkg
    echo   VCPKG_TOOLCHAIN_FILE=path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
    echo.
    echo Example:
    echo   set VCPKG_ROOT=C:\vcpkg
    exit /b 1
)

if not exist "%TOOLCHAIN%" (
    echo ERROR: vcpkg toolchain file not found:
    echo   %TOOLCHAIN%
    exit /b 1
)

REM ============================
REM Optional triplet
REM ============================
REM Your preset should read VCPKG_TARGET_TRIPLET if you want this to matter.
REM Example: set VCPKG_TARGET_TRIPLET=x64-windows-static
REM (No action needed here if it's already set in the environment.)

REM ============================
REM Configure + Build using presets
REM ============================
set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=debug"

cmake --preset "%PRESET%"
if errorlevel 1 exit /b 1

cmake --build --preset "%PRESET%" --parallel
if errorlevel 1 exit /b 1

echo Built: .\%BUILD_DIR%\snake.exe
