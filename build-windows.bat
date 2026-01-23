@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM ============================
REM Defaults
REM ============================
if not defined BUILD_DIR set BUILD_DIR=build
set "GAME_DIR=%BUILD_DIR%\game"

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

REM ============================
REM Resolve vcpkg toolchain
REM ============================
if defined VCPKG_TOOLCHAIN_FILE (
    set "TOOLCHAIN=%VCPKG_TOOLCHAIN_FILE%"
) else if defined VCPKG_ROOT (
    set "TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
) else (
    REM Try to find vcpkg in PATH if not set explicitly
    for /f "delims=" %%I in ('where vcpkg 2^>nul') do (
        set "VCPKG_ROOT=%%~dpI"
        set "TOOLCHAIN=%%~dpIscripts\buildsystems\vcpkg.cmake"
        goto :vcpkg_found
    )

    echo ERROR: vcpkg not configured.
    echo.
    echo Set one of:
    echo   VCPKG_ROOT=path\to\vcpkg
    echo   VCPKG_TOOLCHAIN_FILE=path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
    echo.
    echo Or ensure 'vcpkg' is in your system PATH.
    echo.
    echo Example:
    echo   set VCPKG_ROOT=C:\vcpkg
    exit /b 1
)

:vcpkg_found
if not exist "%TOOLCHAIN%" (
    echo ERROR: vcpkg toolchain file not found:
    echo   %TOOLCHAIN%
    exit /b 1
)

REM ============================
REM Install vcpkg dependencies
REM ============================
if defined VCPKG_ROOT (
    if exist "%VCPKG_ROOT%\vcpkg.exe" (
        echo Installing vcpkg dependencies...
        if defined VCPKG_TARGET_TRIPLET (
            "%VCPKG_ROOT%\vcpkg.exe" install --triplet "%VCPKG_TARGET_TRIPLET%" --x-install-root "%BUILD_DIR%\vcpkg_installed"
        ) else (
            "%VCPKG_ROOT%\vcpkg.exe" install --x-install-root "%BUILD_DIR%\vcpkg_installed"
        )
        if errorlevel 1 (
            echo.
            echo ERROR: vcpkg installation failed.
            echo Press any key to continue anyway, or press Ctrl+C to abort...
            pause
        )
    ) else (
        echo WARN: vcpkg executable not found at %VCPKG_ROOT%\vcpkg.exe (skipping install)
    )
)

REM ============================
REM Configure + Build using presets
REM ============================
set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=debug"

cmake --preset "%PRESET%"
if errorlevel 1 exit /b 1

cmake --build --preset "%PRESET%" --parallel
if errorlevel 1 exit /b 1

REM ============================
REM Build GUI (Nuitka standalone)
REM ============================
if exist "%GAME_DIR%" rmdir /s /q "%GAME_DIR%"
mkdir "%GAME_DIR%"

set "VENV_DIR=launcher\.venv-build"
if not defined PYTHON_BIN set "PYTHON_BIN=py"
if "%PYTHON_BIN%"=="py" (
    %PYTHON_BIN% -3 --version >nul 2>&1
    if errorlevel 1 (
        set "PYTHON_BIN=python"
    )
)

if "%PYTHON_BIN%"=="py" (
    %PYTHON_BIN% -3 -m venv "%VENV_DIR%"
) else (
    %PYTHON_BIN% -m venv "%VENV_DIR%"
)
if errorlevel 1 (
    echo ERROR: Python 3 not found. Install Python and retry.
    exit /b 1
)
call "%VENV_DIR%\Scripts\activate.bat"
python -m pip install --upgrade pip >nul
python -m pip install -r launcher\requirements.txt >nul

echo Building launcher...
set "LAUNCHER_OUT_DIR=%BUILD_DIR%\launcher_build"
python -m nuitka --standalone --assume-yes-for-downloads --lto=no --windows-console-mode=disable --enable-plugin=tk-inter --include-package=customtkinter --output-dir="%LAUNCHER_OUT_DIR%" --output-filename=launcher launcher\main.py
if errorlevel 1 exit /b 1

set "DIST_DIR=%LAUNCHER_OUT_DIR%\main.dist"
if exist "%DIST_DIR%" (
    xcopy /E /I /Y "%DIST_DIR%\*" "%BUILD_DIR%\" >nul
    rmdir /s /q "%LAUNCHER_OUT_DIR%"
)
if exist "launcher\readme.txt" (
    copy /Y "launcher\readme.txt" "%BUILD_DIR%\readme.txt" >nul
)

set "SNAKE_PATH="
if exist "%BUILD_DIR%\snake.exe" set "SNAKE_PATH=%BUILD_DIR%\snake.exe"
if not defined SNAKE_PATH if exist "%BUILD_DIR%\Debug\snake.exe" set "SNAKE_PATH=%BUILD_DIR%\Debug\snake.exe"
if not defined SNAKE_PATH if exist "%BUILD_DIR%\Release\snake.exe" set "SNAKE_PATH=%BUILD_DIR%\Release\snake.exe"
if not defined SNAKE_PATH if exist "%BUILD_DIR%\bin\snake.exe" set "SNAKE_PATH=%BUILD_DIR%\bin\snake.exe"
if not defined SNAKE_PATH if exist "%BUILD_DIR%\Debug\snake\snake.exe" set "SNAKE_PATH=%BUILD_DIR%\Debug\snake\snake.exe"
if not defined SNAKE_PATH if exist "%BUILD_DIR%\Release\snake\snake.exe" set "SNAKE_PATH=%BUILD_DIR%\Release\snake\snake.exe"
if not defined SNAKE_PATH (
    for /r "%BUILD_DIR%" %%F in (snake.exe) do (
        if not defined SNAKE_PATH set "SNAKE_PATH=%%F"
    )
)
if not defined SNAKE_PATH (
    echo Snake binary missing; retrying explicit build target...
    cmake --build --preset "%PRESET%" --parallel --target snake
    if errorlevel 1 exit /b 1
    if exist "%BUILD_DIR%\snake.exe" set "SNAKE_PATH=%BUILD_DIR%\snake.exe"
)

if defined SNAKE_PATH (
    move /Y "%SNAKE_PATH%" "%GAME_DIR%\snake.exe" >nul
) else (
    echo ERROR: missing snake binary after build.
    exit /b 1
)

REM This is an absolute bodge and I don't like it.
echo Copying game assets...
if exist "assets" (
    if not exist "%GAME_DIR%\assets" mkdir "%GAME_DIR%\assets"
    xcopy /E /I /Y "assets\*" "%GAME_DIR%\assets\" >nul
) else (
    echo WARN: assets folder not found. Skipping asset copy.
)
copy /Y "%BUILD_DIR%\*.dll" "%GAME_DIR%\" >nul

echo Built: .\%BUILD_DIR%\launcher.exe
echo Built: .\%GAME_DIR%\snake.exe
